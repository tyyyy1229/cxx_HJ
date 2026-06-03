/*
 * license.cpp
 *
 * BellhopCXX 授权模块实现文件。
 *
 * 本文件实现 include/bhc/license.hpp 中声明的授权接口。
 *
 * 第一版授权目标：
 *
 *     1. 离线授权；
 *     2. 一机一码；
 *     3. 不依赖网络；
 *     4. 不使用复杂 license 文件；
 *     5. 支持涉密机房中人工抄写短机器码和短注册码；
 *     6. 客户输入一次注册码后，可以保存到 license.key；
 *     7. 后续运行时自动读取 license.key。
 *
 * 当前机器码生成逻辑：
 *
 *     CPU_ID + DISK_SERIAL + BOARD_UUID
 *          ↓
 *     字段规范化
 *          ↓
 *     无效值过滤
 *          ↓
 *     固定格式拼接
 *          ↓
 *     SHA256
 *          ↓
 *     Base32 风格编码
 *          ↓
 *     BHC-XXXX-XXXX
 *
 * 当前注册码校验逻辑：
 *
 *     machine_code + product_id + serial_version
 *          ↓
 *     HMAC-SHA256
 *          ↓
 *     Base32 风格编码
 *          ↓
 *     XXXX-XXXX-XXXX
 *
 * 注意：
 *
 *     这里使用的是 HMAC 短注册码方案。
 *
 *     它适合防止普通复制、防止误用、防止客户随意扩散。
 *
 *     但是因为 HMAC secret 会被编译进静态库，所以它不能达到
 *     非对称签名那种强防逆向级别。
 *
 *     如果后续要提高安全性，可以把 HMAC 方案升级为：
 *
 *         私钥生成注册码，公钥在库中验签。
 */

#include "bhc/license.hpp"
#include "util/sha256.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
    /*
     * Windows 下 _popen / _pclose 的头文件由 <cstdio> 提供。
     *
     * 这里不需要额外包含 Windows.h。
     *
     * 当前第一版 Windows 读取硬件 ID 采用 wmic 命令。
     *
     * 优点：
     *
     *     1. 实现简单；
     *     2. 不需要引入 COM / WMI 编程代码；
     *     3. 便于先把授权逻辑跑通。
     *
     * 注意：
     *
     *     新版本 Windows 中 wmic 可能被标记为 deprecated。
     *
     *     后续如果要做更稳定的正式版，可以改为：
     *
     *         1. COM + WMI API；
     *         2. DeviceIoControl；
     *         3. Windows Registry / 系统 API。
     */
#else
    /*
     * Linux 下需要读取 /dev/disk/by-id 的软链接。
     *
     * dirent.h 用于遍历目录。
     * unistd.h 用于 readlink。
     */
    #include <dirent.h>
    #include <unistd.h>
#endif


namespace bhc {
namespace license {

namespace {

/*
 * PRODUCT_ID
 *
 * 产品编号。
 *
 * 作用：
 *
 *     1. 参与机器码原始字符串拼接；
 *     2. 参与注册码 HMAC 消息拼接；
 *     3. 防止不同产品之间注册码混用。
 *
 * 例如：
 *
 *     如果以后你还有 BHC3D、BHCNX2D 等版本，
 *     可以使用不同 PRODUCT_ID。
 */
static const std::string PRODUCT_ID = "BHC2D";


/*
 * MACHINE_CODE_VERSION
 *
 * 机器码算法版本。
 *
 * 作用：
 *
 *     1. 固定当前机器码算法版本；
 *     2. 防止以后算法升级时和旧授权冲突；
 *     3. 如果未来改了硬件字段或拼接方式，可以改为 MC2。
 *
 * 注意：
 *
 *     一旦客户已经使用 MC1 生成过机器码和注册码，
 *     不建议直接修改 MC1 的规则。
 *
 *     如果必须改，应新增 MC2，而不是改 MC1。
 */
static const std::string MACHINE_CODE_VERSION = "MC1";


/*
 * SERIAL_VERSION
 *
 * 注册码算法版本。
 *
 * 作用：
 *
 *     1. 固定当前注册码生成规则；
 *     2. 后续如果修改注册码长度、字符集、HMAC 消息格式，
 *        可以改成 S2；
 *     3. 避免新旧注册码算法混淆。
 */
static const std::string SERIAL_VERSION = "S1";


/*
 * MACHINE_CODE_SALT
 *
 * 机器码生成用的盐值。
 *
 * 作用：
 *
 *     1. 加入机器码原始拼接串；
 *     2. 避免机器码只是硬件信息的简单 Hash；
 *     3. 让不同项目即使硬件字段相同，也能生成不同机器码。
 *
 * 注意：
 *
 *     这个 salt 不是核心安全密钥。
 *
 *     它被编译在库里，不能当作真正秘密。
 *
 *     真正用于注册码防伪的是 SERIAL_SECRET。
 */
static const std::string MACHINE_CODE_SALT = "BHC_MACHINE_CODE_2026";


/*
 * SERIAL_SECRET
 *
 * 注册码 HMAC 使用的内部密钥。
 *
 * 作用：
 *
 *     1. 根据机器码生成理论注册码；
 *     2. 防止外部人员只凭机器码直接推导注册码。
 *
 * 重要说明：
 *
 *     因为当前方案是 HMAC 短注册码，所以这个 secret 会被编译进静态库。
 *
 *     这意味着：
 *
 *         1. 它可以防普通复制；
 *         2. 它可以防止客户随便猜注册码；
 *         3. 但它不能防专业逆向人员从二进制中分析出来。
 *
 *     如果以后要增强安全性，应改成非对称签名方案。
 *
 * 命名建议：
 *
 *     正式交付前，请把这个字符串换成你们项目内部自定义的长随机字符串。
 *
 *     不要使用下面这个示例字符串作为最终交付密钥。
 */
static const std::string SERIAL_SECRET =
    "HEU_College_of_Underwater_Acoustic_Engineering_501";


/*
 * DEFAULT_SERIAL_FILE
 *
 * 默认注册码文件路径。
 *
 * 如果客户没有显式调用 set_serial_file()，
 * check() 会尝试从当前工作目录下的 license.key 读取注册码。
 *
 * 第一版建议文件内容非常简单：
 *
 *     K8D4-9F2Q-3M7X
 *
 * 或者：
 *
 *     SERIAL=K8D4-9F2Q-3M7X
 */
static const std::string DEFAULT_SERIAL_FILE = "license.key";


/*
 * HUMAN_ALPHABET
 *
 * 人工输入友好的 32 字符表。
 *
 * 这里不用普通 Base64，也不用完整十六进制，原因是：
 *
 *     1. Base64 可能包含 + / =，人工输入不友好；
 *     2. 十六进制只有 0-9A-F，虽然简单，但字符空间较小；
 *     3. 这个字符表尽量避免 O 和 0 的混淆；
 *     4. 也避免 I 和 1 的混淆。
 *
 * 字符组成：
 *
 *     A-Z 中去掉 I 和 O，共 24 个字母；
 *     再加 2-9，共 8 个数字；
 *
 *     24 + 8 = 32。
 *
 * 这样正好可以做 5 bit 编码。
 */
static const std::string HUMAN_ALPHABET =
    "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";


/*
 * g_mutex
 *
 * 授权模块内部互斥锁。
 *
 * 作用：
 *
 *     1. 保护全局缓存变量；
 *     2. 防止多线程同时调用 check()、set_serial()、clear_cache()
 *        时发生状态错乱；
 *     3. 保证授权校验状态读写一致。
 *
 * 为什么需要？
 *
 *     bhc::run() 可能在客户程序中被多线程调用。
 *
 *     如果不加锁，多个线程可能同时读取 license.key、
 *     同时修改 g_checked / g_valid / g_last_error。
 */
static std::mutex g_mutex;


/*
 * g_serial_normalized
 *
 * 内存中保存的注册码。
 *
 * 这里保存的是“规范化后的注册码”，不是用户原始输入。
 *
 * 例如用户输入：
 *
 *     k8d4-9f2q-3m7x
 *
 * 规范化后保存为：
 *
 *     K8D49F2Q3M7X
 *
 * 为什么要保存规范化版本？
 *
 *     1. 后续比较更简单；
 *     2. 可以兼容横杠、空格、小写；
 *     3. 避免同一注册码因为格式不同而判断失败。
 */
static std::string g_serial_normalized;


/*
 * g_serial_file
 *
 * 当前设置的注册码文件路径。
 *
 * 默认是 license.key。
 *
 * 如果客户调用：
 *
 *     bhc::license::set_serial_file("/path/to/license.key");
 *
 * 那么这里会被更新为客户指定路径。
 */
static std::string g_serial_file = DEFAULT_SERIAL_FILE;


/*
 * g_checked
 *
 * 表示是否已经执行过授权校验。
 *
 * 作用：
 *
 *     1. 避免每次调用 bhc::run() 都重复读取硬件信息；
 *     2. 避免每次调用 bhc::run() 都重复读取 license.key；
 *     3. 降低运行时开销。
 *
 * 逻辑：
 *
 *     g_checked == false：
 *
 *         说明还没有校验过，或者缓存已经被清除。
 *
 *     g_checked == true：
 *
 *         说明已经校验过，可以直接返回 g_valid。
 */
static bool g_checked = false;


/*
 * g_valid
 *
 * 保存最近一次授权校验结果。
 *
 * 只有当 g_checked == true 时，g_valid 才有实际意义。
 *
 * 逻辑：
 *
 *     g_checked == true && g_valid == true：
 *
 *         授权已校验并通过。
 *
 *     g_checked == true && g_valid == false：
 *
 *         授权已校验但失败。
 */
static bool g_valid = false;


/*
 * g_last_error
 *
 * 保存最近一次授权相关错误信息。
 *
 * 例如：
 *
 *     1. 无法读取主板 UUID；
 *     2. 硬件 ID 不足；
 *     3. license.key 不存在；
 *     4. 注册码格式错误；
 *     5. 注册码与当前机器不匹配。
 *
 * last_error() 会返回这个变量。
 */
static std::string g_last_error;


/*
 * g_cached_machine_code
 *
 * 缓存当前机器生成的机器码。
 *
 * 作用：
 *
 *     1. 避免重复读取 CPU / 硬盘 / 主板信息；
 *     2. 保证同一次运行过程中机器码结果稳定；
 *     3. get_machine_code() 和 check() 可以共享结果。
 *
 * 注意：
 *
 *     clear_cache() 会清空它。
 */
static std::string g_cached_machine_code;


/*
 * trim()
 *
 * 功能：
 *
 *     去掉字符串首尾空白字符。
 *
 * 参数：
 *
 *     text：
 *
 *         输入字符串。
 *
 * 返回值：
 *
 *     去掉首尾空白后的字符串。
 *
 * 空白字符包括：
 *
 *     空格、制表符、换行符、回车符等。
 *
 * 使用场景：
 *
 *     1. 读取命令输出；
 *     2. 读取文件内容；
 *     3. 解析 SERIAL=xxxx；
 *     4. 清理硬件 ID。
 */
std::string trim(const std::string& text)
{
    /*
     * begin 用于寻找第一个非空白字符的位置。
     */
    std::size_t begin = 0;

    /*
     * 从前往后跳过所有空白字符。
     *
     * static_cast<unsigned char> 是为了避免 char 为负值时
     * 传给 std::isspace 产生未定义行为。
     */
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }

    /*
     * 如果整个字符串都是空白，则返回空字符串。
     */
    if (begin == text.size()) {
        return std::string();
    }

    /*
     * end 指向最后一个字符的后一位。
     */
    std::size_t end = text.size();

    /*
     * 从后往前跳过空白字符。
     */
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }

    /*
     * substr(begin, end - begin) 返回有效内容部分。
     */
    return text.substr(begin, end - begin);
}


/*
 * to_upper()
 *
 * 功能：
 *
 *     将字符串转换为大写。
 *
 * 参数：
 *
 *     text：
 *
 *         输入字符串。
 *
 * 返回值：
 *
 *     全部转为大写后的字符串。
 *
 * 用途：
 *
 *     1. 规范化硬件 ID；
 *     2. 规范化注册码；
 *     3. 保证大小写不影响校验结果。
 */
std::string to_upper(const std::string& text)
{
    /*
     * result 保存转换后的结果。
     */
    std::string result = text;

    /*
     * 逐字符转换为大写。
     */
    for (char& ch : result) {
        ch = static_cast<char>(
            std::toupper(static_cast<unsigned char>(ch))
        );
    }

    return result;
}


/*
 * normalize_hardware_id()
 *
 * 功能：
 *
 *     对 CPU_ID / DISK_SERIAL / BOARD_UUID 做统一规范化。
 *
 * 为什么要规范化？
 *
 *     不同系统、不同命令返回的硬件 ID 格式可能不同。
 *
 *     例如主板 UUID：
 *
 *         4c4c4544-0039-3710-8051-c3c04f4e3332
 *
 *     也可能返回：
 *
 *         4C4C4544003937108051C3C04F4E3332
 *
 *     如果不规范化，同一台机器可能生成不同机器码。
 *
 * 规范化规则：
 *
 *     1. 转大写；
 *     2. 去掉空格；
 *     3. 去掉横杠；
 *     4. 去掉冒号；
 *     5. 去掉下划线；
 *     6. 只保留字母和数字。
 *
 * 参数：
 *
 *     value：
 *
 *         原始硬件 ID。
 *
 * 返回值：
 *
 *     规范化后的硬件 ID。
 */
std::string normalize_hardware_id(const std::string& value)
{
    /*
     * upper 是大写后的字符串。
     */
    const std::string upper = to_upper(trim(value));

    /*
     * result 保存最终规范化结果。
     */
    std::string result;

    /*
     * 预留空间，减少字符串扩容次数。
     */
    result.reserve(upper.size());

    /*
     * 逐字符处理。
     */
    for (char ch : upper) {
        /*
         * 字母和数字保留。
         *
         * 其他字符，例如：
         *
         *     空格
         *     -
         *     :
         *     _
         *     .
         *
         * 都丢弃。
         */
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            result.push_back(ch);
        }
    }

    return result;
}


/*
 * is_invalid_hardware_id()
 *
 * 功能：
 *
 *     判断规范化后的硬件 ID 是否无效。
 *
 * 为什么需要？
 *
 *     很多机器可能返回默认值，例如：
 *
 *         To Be Filled By O.E.M.
 *         Default String
 *         00000000-0000-0000-0000-000000000000
 *
 *     这些值不能用于生成机器码。
 *
 * 参数：
 *
 *     normalized：
 *
 *         已经经过 normalize_hardware_id() 处理后的字符串。
 *
 * 返回值：
 *
 *     true：
 *
 *         表示该 ID 无效，不能参与机器码生成。
 *
 *     false：
 *
 *         表示该 ID 可以参与机器码生成。
 */
bool is_invalid_hardware_id(const std::string& normalized)
{
    /*
     * 空字符串无效。
     */
    if (normalized.empty()) {
        return true;
    }

    /*
     * 常见无效默认值列表。
     *
     * 注意：
     *
     *     这里的值都应该是规范化后的形式。
     *
     *     例如：
     *
     *         "TO BE FILLED BY O.E.M."
     *
     *     经过 normalize_hardware_id() 后会变成：
     *
     *         "TOBEFILLEDBYOEM"
     */
    static const std::vector<std::string> invalid_values = {
        "EMPTY",
        "UNKNOWN",
        "NONE",
        "NULL",
        "NAN",
        "TOBEFILLEDBYOEM",
        "TOBEFILLED",
        "DEFAULTSTRING",
        "SYSTEMSERIALNUMBER",
        "BASEBOARDSERIALNUMBER",
        "NOTSPECIFIED",
        "INVALID",
        "OEM"
    };

    /*
     * 如果命中常见无效值，则判为无效。
     */
    for (const std::string& invalid : invalid_values) {
        if (normalized == invalid) {
            return true;
        }
    }

    /*
     * all_same 用于判断字符串是否由同一个字符重复构成。
     *
     * 例如：
     *
     *     0000000000000000
     *     FFFFFFFFFFFFFFFF
     *
     * 这类值通常也是无效 ID。
     */
    bool all_same = true;

    /*
     * 从第二个字符开始，和第一个字符比较。
     */
    for (std::size_t i = 1; i < normalized.size(); ++i) {
        if (normalized[i] != normalized[0]) {
            all_same = false;
            break;
        }
    }

    /*
     * 如果长度大于等于 8，并且全部字符相同，
     * 基本可以认为是默认占位值。
     */
    if (normalized.size() >= 8 && all_same) {
        return true;
    }

    /*
     * UUID 全 0 的特殊情况。
     *
     * 规范化后通常是 32 个 0。
     */
    if (normalized == "00000000000000000000000000000000") {
        return true;
    }

    /*
     * UUID 全 F 的特殊情况。
     */
    if (normalized == "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF") {
        return true;
    }

    /*
     * 没有命中任何无效规则，认为有效。
     */
    return false;
}


/*
 * safe_hardware_id()
 *
 * 功能：
 *
 *     对原始硬件 ID 做规范化和无效值过滤。
 *
 * 参数：
 *
 *     raw：
 *
 *         原始硬件 ID。
 *
 * 返回值：
 *
 *     如果有效：
 *
 *         返回规范化后的硬件 ID。
 *
 *     如果无效：
 *
 *         返回 "EMPTY"。
 *
 * 为什么返回 EMPTY 而不是空字符串？
 *
 *     因为机器码拼接格式要求字段位置固定。
 *
 *     即使某个字段取不到，也要拼成：
 *
 *         CPU=EMPTY
 *
 *     而不是直接删除 CPU 字段。
 */
std::string safe_hardware_id(const std::string& raw)
{
    /*
     * 先规范化。
     */
    const std::string normalized = normalize_hardware_id(raw);

    /*
     * 再判断是否无效。
     */
    if (is_invalid_hardware_id(normalized)) {
        return "EMPTY";
    }

    /*
     * 有效则返回规范化值。
     */
    return normalized;
}


/*
 * is_effective_id()
 *
 * 功能：
 *
 *     判断字段是否是有效硬件 ID。
 *
 * 参数：
 *
 *     value：
 *
 *         safe_hardware_id() 返回的字段。
 *
 * 返回值：
 *
 *     true：
 *
 *         字段有效。
 *
 *     false：
 *
 *         字段为 EMPTY，无效。
 */
bool is_effective_id(const std::string& value)
{
    return !value.empty() && value != "EMPTY";
}


/*
 * read_text_file()
 *
 * 功能：
 *
 *     读取文本文件全部内容。
 *
 * 参数：
 *
 *     path：
 *
 *         文件路径。
 *
 *     out：
 *
 *         输出参数，保存读取到的文件内容。
 *
 * 返回值：
 *
 *     true：
 *
 *         文件打开并读取成功。
 *
 *     false：
 *
 *         文件不存在、无法打开、没有权限等。
 */
bool read_text_file(const std::string& path, std::string& out)
{
    /*
     * 使用二进制方式打开，避免不同平台换行符转换影响。
     */
    std::ifstream ifs(path.c_str(), std::ios::in | std::ios::binary);

    /*
     * 如果文件打开失败，直接返回 false。
     */
    if (!ifs) {
        return false;
    }

    /*
     * 使用 stringstream 读取整个文件。
     */
    std::ostringstream oss;
    oss << ifs.rdbuf();

    /*
     * 将读取结果写入输出参数。
     */
    out = oss.str();

    return true;
}


/*
 * write_text_file()
 *
 * 功能：
 *
 *     写入文本文件。
 *
 * 参数：
 *
 *     path：
 *
 *         文件路径。
 *
 *     text：
 *
 *         要写入的文本内容。
 *
 * 返回值：
 *
 *     true：
 *
 *         写入成功。
 *
 *     false：
 *
 *         文件无法创建、没有权限、磁盘不可写等。
 */
bool write_text_file(const std::string& path, const std::string& text)
{
    /*
     * 使用二进制方式写入，避免不同平台换行符自动转换。
     */
    std::ofstream ofs(path.c_str(), std::ios::out | std::ios::binary);

    /*
     * 如果文件打开失败，返回 false。
     */
    if (!ofs) {
        return false;
    }

    /*
     * 写入文本。
     */
    ofs << text;

    /*
     * 检查输出流状态。
     */
    return static_cast<bool>(ofs);
}


/*
 * run_command()
 *
 * 功能：
 *
 *     执行系统命令并读取标准输出。
 *
 * 参数：
 *
 *     command：
 *
 *         要执行的命令。
 *
 * 返回值：
 *
 *     命令标准输出内容。
 *
 * 使用场景：
 *
 *     Windows 第一版使用 wmic 读取硬件 ID。
 *
 * 注意：
 *
 *     这个函数只用于读取本机硬件标识。
 *
 *     command 不应该来自用户输入，避免命令注入风险。
 */
std::string run_command(const std::string& command)
{
    /*
     * result 保存命令输出。
     */
    std::string result;

    /*
     * pipe 是命令输出管道。
     *
     * Windows 使用 _popen；
     * Linux 使用 popen。
     */
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif

    /*
     * 如果管道创建失败，返回空字符串。
     */
    if (!pipe) {
        return result;
    }

    /*
     * buffer 是临时读取缓冲区。
     *
     * 每次从管道中最多读取 255 个字符，
     * 最后一个位置留给字符串结束符。
     */
    std::array<char, 256> buffer;

    /*
     * 循环读取命令输出。
     */
    while (std::fgets(buffer.data(),
                     static_cast<int>(buffer.size()),
                     pipe) != nullptr) {
        result += buffer.data();
    }

    /*
     * 关闭管道。
     */
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    return result;
}


/*
 * parse_key_value_output()
 *
 * 功能：
 *
 *     从类似 key=value 的命令输出中提取 value。
 *
 * 参数：
 *
 *     output：
 *
 *         命令输出文本。
 *
 *     key：
 *
 *         要查找的键名。
 *
 * 示例：
 *
 *     output:
 *
 *         ProcessorId=BFEBFBFF000806EA
 *
 *     key:
 *
 *         ProcessorId
 *
 *     返回：
 *
 *         BFEBFBFF000806EA
 *
 * 使用场景：
 *
 *     Windows wmic /value 输出解析。
 */
std::string parse_key_value_output(const std::string& output,
                                   const std::string& key)
{
    /*
     * 使用字符串流按行读取。
     */
    std::istringstream iss(output);

    /*
     * line 保存当前读取的一行。
     */
    std::string line;

    /*
     * prefix 是要匹配的前缀，例如 "ProcessorId="。
     */
    const std::string prefix = key + "=";

    /*
     * 逐行查找。
     */
    while (std::getline(iss, line)) {
        /*
         * 去掉行首尾空白。
         */
        line = trim(line);

        /*
         * 如果该行以 prefix 开头，则取 prefix 后面的内容。
         */
        if (line.find(prefix) == 0) {
            return trim(line.substr(prefix.size()));
        }
    }

    /*
     * 未找到则返回空字符串。
     */
    return std::string();
}


/*
 * parse_first_nonempty_line()
 *
 * 功能：
 *
 *     从多行文本中取第一行非空内容。
 *
 * 使用场景：
 *
 *     有些命令输出不是 key=value，而是：
 *
 *         SerialNumber
 *         XXXXXXXX
 *
 *     或者文件内容只有一行。
 */
std::string parse_first_nonempty_line(const std::string& text)
{
    std::istringstream iss(text);
    std::string line;

    while (std::getline(iss, line)) {
        line = trim(line);

        if (!line.empty()) {
            return line;
        }
    }

    return std::string();
}


#ifdef _WIN32

/*
 * read_cpu_id_raw()
 *
 * Windows 读取 CPU ID。
 *
 * 第一版使用 wmic：
 *
 *     wmic cpu get ProcessorId /value
 *
 * 典型输出：
 *
 *     ProcessorId=BFEBFBFF000806EA
 *
 * 返回值：
 *
 *     原始 CPU ID 字符串。
 */
std::string read_cpu_id_raw()
{
    const std::string output =
        run_command("wmic cpu get ProcessorId /value 2>nul");

    return parse_key_value_output(output, "ProcessorId");
}


/*
 * read_disk_serial_raw()
 *
 * Windows 读取硬盘序列号。
 *
 * 第一版使用 wmic：
 *
 *     wmic diskdrive get SerialNumber /value
 *
 * 如果机器有多块硬盘，输出可能包含多个 SerialNumber。
 *
 * 第一版取第一个有效序列号。
 */
std::string read_disk_serial_raw()
{
    const std::string output =
        run_command("wmic diskdrive get SerialNumber /value 2>nul");

    /*
     * 按 key=value 解析第一条 SerialNumber。
     */
    return parse_key_value_output(output, "SerialNumber");
}


/*
 * read_board_uuid_raw()
 *
 * Windows 读取主板 / 系统 UUID。
 *
 * 第一版使用：
 *
 *     wmic csproduct get UUID /value
 *
 * 典型输出：
 *
 *     UUID=4C4C4544-0039-3710-8051-C3C04F4E3332
 */
std::string read_board_uuid_raw()
{
    const std::string output =
        run_command("wmic csproduct get UUID /value 2>nul");

    return parse_key_value_output(output, "UUID");
}

#else

/*
 * read_linux_file_first_line()
 *
 * Linux 辅助函数：
 *
 *     读取指定文件的第一行非空内容。
 *
 * 参数：
 *
 *     path：
 *
 *         文件路径。
 *
 * 返回值：
 *
 *     第一行非空内容。
 *
 *     如果文件不存在或无法读取，返回空字符串。
 */
std::string read_linux_file_first_line(const std::string& path)
{
    std::string content;

    if (!read_text_file(path, content)) {
        return std::string();
    }

    return parse_first_nonempty_line(content);
}


/*
 * find_cpuinfo_value()
 *
 * Linux 辅助函数：
 *
 *     从 /proc/cpuinfo 中查找指定字段。
 *
 * 参数：
 *
 *     key：
 *
 *         字段名，例如：
 *
 *             Serial
 *             vendor_id
 *             cpu family
 *             model
 *             stepping
 *
 * 返回值：
 *
 *     找到的字段值。
 */
std::string find_cpuinfo_value(const std::string& key)
{
    std::string content;

    if (!read_text_file("/proc/cpuinfo", content)) {
        return std::string();
    }

    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line)) {
        /*
         * /proc/cpuinfo 的典型格式：
         *
         *     key : value
         */
        const std::size_t pos = line.find(':');

        if (pos == std::string::npos) {
            continue;
        }

        const std::string left = trim(line.substr(0, pos));
        const std::string right = trim(line.substr(pos + 1));

        if (left == key) {
            return right;
        }
    }

    return std::string();
}


/*
 * read_cpu_id_raw()
 *
 * Linux 读取 CPU ID。
 *
 * 注意：
 *
 *     x86 Linux 通常没有真正唯一的 CPU Serial。
 *
 *     ARM 平台上 /proc/cpuinfo 可能有 Serial 字段。
 *
 * 第一优先级：
 *
 *     /proc/cpuinfo 中的 Serial 字段。
 *
 * 第二优先级：
 *
 *     vendor_id + cpu family + model + stepping
 *
 *     这不是严格唯一 ID，只作为辅助字段参与机器码。
 *
 * 返回值：
 *
 *     原始 CPU 标识字符串。
 */
std::string read_cpu_id_raw()
{
    /*
     * ARM 或部分平台可能存在 Serial 字段。
     */
    const std::string serial = find_cpuinfo_value("Serial");

    if (!serial.empty()) {
        return serial;
    }

    /*
     * x86 上常见字段组合。
     *
     * 这些字段不一定唯一，但可以作为 CPU 辅助特征。
     */
    const std::string vendor = find_cpuinfo_value("vendor_id");
    const std::string family = find_cpuinfo_value("cpu family");
    const std::string model = find_cpuinfo_value("model");
    const std::string stepping = find_cpuinfo_value("stepping");

    /*
     * 如果 vendor 都没有，说明 /proc/cpuinfo 不可用或格式异常。
     */
    if (vendor.empty()) {
        return std::string();
    }

    /*
     * 拼接成一个 CPU 辅助标识。
     */
    return vendor + "_" + family + "_" + model + "_" + stepping;
}


/*
 * read_disk_serial_from_sys_block()
 *
 * Linux 从 /sys/block 中读取磁盘序列号。
 *
 * 常见路径：
 *
 *     /sys/block/sda/device/serial
 *     /sys/block/sdb/device/serial
 *     /sys/block/nvme0n1/device/serial
 *
 * 第一版为了简单，尝试若干常见系统盘名称。
 */
std::string read_disk_serial_from_sys_block()
{
    /*
     * candidates 是常见磁盘设备名列表。
     *
     * 顺序代表优先级。
     *
     * nvme0n1：
     *
     *     常见 NVMe 系统盘。
     *
     * sda：
     *
     *     常见 SATA / SCSI 系统盘。
     *
     * vda：
     *
     *     常见虚拟机 virtio 磁盘。
     *
     * hda：
     *
     *     较老 IDE 磁盘。
     */
    const std::vector<std::string> candidates = {
    "nvme0n1",
    "nvme1n1",
    "sda",
    "sdb",
    "sdc",
    "vda",
    "vdb",
    "xvda",
    "xvdb",
    "hda",
    "hdb",
    "mmcblk0"
};

    for (const std::string& dev : candidates) {
        const std::string path = "/sys/block/" + dev + "/device/serial";

        const std::string serial = read_linux_file_first_line(path);

        if (!serial.empty()) {
            return serial;
        }
    }

    return std::string();
}


/*
 * read_disk_serial_from_by_id()
 *
 * Linux 从 /dev/disk/by-id 中寻找磁盘标识。
 *
 * /dev/disk/by-id 中通常有类似：
 *
 *     ata-Samsung_SSD_860_EVO_500GB_S3Z...
 *     nvme-Samsung_SSD_970_EVO_...
 *
 * 这些名字通常包含厂商、型号、序列号。
 *
 * 虽然它不是纯序列号，但作为机器码输入是可用的。
 */
std::string read_disk_serial_from_by_id()
{
    const char* dir_path = "/dev/disk/by-id";

    DIR* dir = opendir(dir_path);

    if (!dir) {
        return std::string();
    }

    std::string best;

    while (true) {
        dirent* entry = readdir(dir);

        if (!entry) {
            break;
        }

        const std::string name = entry->d_name;

        /*
         * 跳过当前目录和上级目录。
         */
        if (name == "." || name == "..") {
            continue;
        }

        /*
         * 跳过分区项。
         *
         * 例如：
         *
         *     ata-xxx-part1
         *
         * 分区不是磁盘本体，优先不用。
         */
        if (name.find("part") != std::string::npos) {
            continue;
        }

        /*
         * 优先使用 ata / nvme / scsi 开头的磁盘 ID。
         */
        if (name.find("ata-") == 0 ||
            name.find("nvme-") == 0 ||
            name.find("scsi-") == 0) {
            best = name;
            break;
        }

        /*
         * 如果暂时没有更好的候选，就保存第一个非分区项。
         */
        if (best.empty()) {
            best = name;
        }
    }

    closedir(dir);

    return best;
}


/*
 * read_disk_serial_raw()
 *
 * Linux 读取硬盘序列号。
 *
 * 第一优先级：
 *
 *     /sys/block/.../device/serial
 *
 * 第二优先级：
 *
 *     /dev/disk/by-id
 */
std::string read_disk_serial_raw()
{
    const std::string from_sys = read_disk_serial_from_sys_block();

    if (!from_sys.empty()) {
        return from_sys;
    }

    return read_disk_serial_from_by_id();
}


/*
 * read_board_uuid_raw()
 *
 * Linux 读取主板 / 系统 UUID。
 *
 * 第一优先级：
 *
 *     /sys/class/dmi/id/product_uuid
 *
 * 第二优先级：
 *
 *     /sys/class/dmi/id/board_serial
 *
 * 注意：
 *
 *     dmidecode 通常需要 root 权限，所以第一版不依赖 dmidecode。
 */
std::string read_board_uuid_raw()
{
    const std::vector<std::string> paths = {
        "/sys/class/dmi/id/product_uuid",
        "/sys/class/dmi/id/board_serial",
        "/sys/class/dmi/id/product_serial",
        "/sys/class/dmi/id/chassis_serial"
    };

    for (const std::string& path : paths) {
        const std::string value = read_linux_file_first_line(path);

        if (!value.empty()) {
            return value;
        }
    }

    return std::string();
}   

#endif


/*
 * encode_human_base32()
 *
 * 功能：
 *
 *     将二进制字节编码为人工友好的 32 字符表字符串。
 *
 * 参数：
 *
 *     bytes：
 *
 *         输入二进制字节数组。
 *
 *     output_length：
 *
 *         希望输出的字符数量。
 *
 * 返回值：
 *
 *     编码后的字符串。
 *
 * 编码逻辑：
 *
 *     每 5 个 bit 可以表示 0 到 31。
 *
 *     HUMAN_ALPHABET 正好有 32 个字符。
 *
 *     因此可以把输入字节流拆成一组组 5 bit，
 *     每组映射到一个字符。
 *
 * 为什么要指定 output_length？
 *
 *     机器码只需要 8 个有效字符；
 *     注册码只需要 12 个有效字符。
 *
 *     所以没必要输出完整 Hash。
 */
std::string encode_human_base32(const bhc::util::Bytes& bytes,
                                std::size_t output_length)
{
    /*
     * result 保存输出字符串。
     */
    std::string result;

    /*
     * 提前预留空间，避免频繁扩容。
     */
    result.reserve(output_length);

    /*
     * bit_buffer 是临时 bit 缓冲区。
     *
     * 每读入一个字节，就把它追加到 bit_buffer 低位。
     */
    std::uint32_t bit_buffer = 0;

    /*
     * bit_count 表示 bit_buffer 当前保存了多少个有效 bit。
     */
    int bit_count = 0;

    /*
     * byte_index 表示当前读到 bytes 的第几个字节。
     */
    std::size_t byte_index = 0;

    /*
     * 只要输出长度还没达到要求，就继续编码。
     */
    while (result.size() < output_length) {
        /*
         * 如果 bit_buffer 中不足 5 个 bit，
         * 就继续从输入字节数组中读一个字节。
         */
        if (bit_count < 5) {
            /*
             * 如果输入字节用完了，但输出还不够，
             * 就补 0。
             *
             * 对 SHA256 / HMAC-SHA256 来说，输入一般有 32 字节，
             * 生成 8 或 12 个字符完全足够。
             *
             * 这里补 0 只是防御性处理。
             */
            const std::uint8_t next_byte =
                (byte_index < bytes.size()) ? bytes[byte_index] : 0;

            if (byte_index < bytes.size()) {
                ++byte_index;
            }

            /*
             * 将新字节追加到 bit_buffer。
             *
             * 左移 8 位表示给新字节腾出位置。
             */
            bit_buffer = (bit_buffer << 8u) | next_byte;
            bit_count += 8;
        }

        /*
         * 从 bit_buffer 中取最高的 5 个有效 bit。
         *
         * index 范围一定是 0 到 31。
         */
        const int shift = bit_count - 5;
        const std::uint32_t index = (bit_buffer >> shift) & 0x1fu;

        /*
         * 把 5 bit 对应的 index 映射为人工友好字符。
         */
        result.push_back(HUMAN_ALPHABET[index]);

        /*
         * 已经消费掉 5 个 bit。
         */
        bit_count -= 5;

        /*
         * 清掉已经消费过的高位，只保留剩余 bit。
         *
         * 如果 bit_count 为 0，则 bit_buffer 清零。
         */
        if (bit_count == 0) {
            bit_buffer = 0;
        } else {
            const std::uint32_t mask = (1u << bit_count) - 1u;
            bit_buffer &= mask;
        }
    }

    return result;
}


/*
 * group_code()
 *
 * 功能：
 *
 *     将连续字符串按固定长度分组，并用横杠连接。
 *
 * 参数：
 *
 *     text：
 *
 *         未分组字符串。
 *
 *     group_size：
 *
 *         每组字符数量。
 *
 * 返回值：
 *
 *     分组后的字符串。
 *
 * 示例：
 *
 *     text = "ABCDEFGH"
 *     group_size = 4
 *
 *     返回：
 *
 *         "ABCD-EFGH"
 */
std::string group_code(const std::string& text, std::size_t group_size)
{
    /*
     * 如果 group_size 为 0，无法分组，直接返回原字符串。
     */
    if (group_size == 0) {
        return text;
    }

    std::string result;

    /*
     * 预留空间。
     *
     * text.size() 是原字符数。
     * text.size() / group_size 是大致需要增加的横杠数。
     */
    result.reserve(text.size() + text.size() / group_size);

    /*
     * 逐字符复制。
     */
    for (std::size_t i = 0; i < text.size(); ++i) {
        /*
         * 每到一个新的分组，并且不是第一个字符，就插入横杠。
         */
        if (i > 0 && i % group_size == 0) {
            result.push_back('-');
        }

        result.push_back(text[i]);
    }

    return result;
}


/*
 * format_machine_code()
 *
 * 功能：
 *
 *     将 8 位机器码主体格式化为：
 *
 *         BHC-XXXX-XXXX
 *
 * 参数：
 *
 *     body：
 *
 *         机器码主体。
 *
 *         例如：
 *
 *             7D92F3A1
 *
 * 返回值：
 *
 *     带 BHC 前缀和横杠分组的机器码。
 */
std::string format_machine_code(const std::string& body)
{
    return "BHC-" + group_code(body, 4);
}


/*
 * format_serial()
 *
 * 功能：
 *
 *     将 12 位注册码主体格式化为：
 *
 *         XXXX-XXXX-XXXX
 *
 * 参数：
 *
 *     normalized_serial：
 *
 *         12 位无横杠注册码。
 *
 * 返回值：
 *
 *     分组后的注册码。
 */
std::string format_serial(const std::string& normalized_serial)
{
    return group_code(normalized_serial, 4);
}


/*
 * normalize_serial()
 *
 * 功能：
 *
 *     对用户输入的注册码进行规范化。
 *
 * 规范化规则：
 *
 *     1. 去掉空格；
 *     2. 去掉横杠；
 *     3. 转大写；
 *     4. 只保留字母数字；
 *     5. 检查是否都属于 HUMAN_ALPHABET。
 *
 * 参数：
 *
 *     serial：
 *
 *         用户输入的原始注册码。
 *
 * 返回值：
 *
 *     规范化后的注册码。
 *
 *     如果有非法字符，也会先返回清理后的结果；
 *     是否真正有效由 is_valid_serial_format() 判断。
 */
std::string normalize_serial(const std::string& serial)
{
    const std::string upper = to_upper(trim(serial));

    std::string result;
    result.reserve(upper.size());

    for (char ch : upper) {
        /*
         * 只保留字母和数字。
         *
         * 横杠、空格等会被忽略。
         */
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            result.push_back(ch);
        }
    }

    return result;
}


/*
 * is_valid_serial_format()
 *
 * 功能：
 *
 *     检查规范化后的注册码格式是否正确。
 *
 * 当前第一版规则：
 *
 *     1. 长度必须是 12；
 *     2. 每个字符必须属于 HUMAN_ALPHABET。
 *
 * 参数：
 *
 *     normalized_serial：
 *
 *         已经去掉横杠、空格并转大写的注册码。
 *
 * 返回值：
 *
 *     true：
 *
 *         格式正确。
 *
 *     false：
 *
 *         格式错误。
 */
bool is_valid_serial_format(const std::string& normalized_serial)
{
    /*
     * 第一版注册码主体固定 12 位。
     */
    if (normalized_serial.size() != 12) {
        return false;
    }

    /*
     * 检查每个字符是否属于人工友好字符表。
     */
    for (char ch : normalized_serial) {
        if (HUMAN_ALPHABET.find(ch) == std::string::npos) {
            return false;
        }
    }

    return true;
}


/*
 * parse_serial_file_content()
 *
 * 功能：
 *
 *     从 license.key 文件内容中解析注册码。
 *
 * 支持格式 1：
 *
 *     K8D4-9F2Q-3M7X
 *
 * 支持格式 2：
 *
 *     SERIAL=K8D4-9F2Q-3M7X
 *
 * 参数：
 *
 *     content：
 *
 *         license.key 文件全部内容。
 *
 * 返回值：
 *
 *     提取出的原始注册码字符串。
 */
std::string parse_serial_file_content(const std::string& content)
{
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line)) {
        line = trim(line);

        /*
         * 跳过空行。
         */
        if (line.empty()) {
            continue;
        }

        /*
         * 支持注释行。
         *
         * 如果客户或你们自己在 license.key 中写：
         *
         *     # comment
         *
         * 则忽略。
         */
        if (!line.empty() && line[0] == '#') {
            continue;
        }

        /*
         * 查找 SERIAL= 格式。
         */
        const std::string upper_line = to_upper(line);
        const std::string prefix = "SERIAL=";

        if (upper_line.find(prefix) == 0) {
            return trim(line.substr(prefix.size()));
        }

        /*
         * 如果不是 SERIAL=，则认为这一行本身就是注册码。
         */
        return line;
    }

    return std::string();
}


/*
 * build_machine_raw_string()
 *
 * 功能：
 *
 *     根据规范化后的 CPU / DISK / BOARD 字段构造机器码原始串。
 *
 * 参数：
 *
 *     cpu：
 *
 *         规范化后的 CPU 标识。
 *
 *     disk：
 *
 *         规范化后的硬盘标识。
 *
 *     board：
 *
 *         规范化后的主板 UUID。
 *
 * 返回值：
 *
 *     固定格式的机器码原始拼接串。
 *
 * 为什么要带字段名？
 *
 *     不建议简单拼成：
 *
 *         CPU + DISK + BOARD
 *
 *     因为字段边界不清晰。
 *
 *     使用：
 *
 *         CPU=...|DISK=...|BOARD=...
 *
 *     可以保证格式清楚，后续调试也方便。
 */
std::string build_machine_raw_string(const std::string& cpu,
                                     const std::string& disk,
                                     const std::string& board)
{
    std::ostringstream oss;

    oss << "PRODUCT=" << PRODUCT_ID
        << "|MCVER=" << MACHINE_CODE_VERSION
        << "|CPU=" << cpu
        << "|DISK=" << disk
        << "|BOARD=" << board
        << "|SALT=" << MACHINE_CODE_SALT;

    return oss.str();
}


/*
 * generate_machine_code_unlocked()
 *
 * 功能：
 *
 *     生成当前机器码。
 *
 * 为什么函数名带 unlocked？
 *
 *     表示该函数内部不加锁。
 *
 *     外层 public 接口 get_machine_code() 会加 g_mutex。
 *
 *     check() 已经持有 g_mutex 时也需要调用机器码生成逻辑。
 *
 *     如果内部再加锁，可能造成死锁。
 *
 * 返回值：
 *
 *     成功：
 *
 *         BHC-XXXX-XXXX
 *
 *     失败：
 *
 *         空字符串。
 *
 * 失败时：
 *
 *     会设置 g_last_error。
 */
std::string generate_machine_code_unlocked()
{
    /*
     * 如果已经有缓存机器码，直接返回。
     */
    if (!g_cached_machine_code.empty()) {
        return g_cached_machine_code;
    }

    /*
     * 读取原始硬件 ID。
     *
     * 这三个函数内部会根据 Windows / Linux 分别实现。
     */
    const std::string cpu_raw = read_cpu_id_raw();
    const std::string disk_raw = read_disk_serial_raw();
    const std::string board_raw = read_board_uuid_raw();

    /*
     * 对原始硬件 ID 进行规范化和无效过滤。
     *
     * 如果字段无效，会返回 EMPTY。
     */
    const std::string cpu = safe_hardware_id(cpu_raw);
    const std::string disk = safe_hardware_id(disk_raw);
    const std::string board = safe_hardware_id(board_raw);

    /*
     * 判断主板 UUID 是否有效。
     *
     * 前面我们已经定了第一版规则：
     *
     *     BOARD 必须有效；
     *     CPU / DISK 至少一个有效。
     *
     * 原因：
     *
     *     主板 UUID 通常最能代表一台机器。
     * 
     * 新：有效性规则：
        CPU / DISK / BOARD 至少两个有效

     */
    int effective_count = 0;

    if (is_effective_id(cpu)) {
        ++effective_count;
    }

    if (is_effective_id(disk)) {
        ++effective_count;
    }

    if (is_effective_id(board)) {
        ++effective_count;
    }

    if (effective_count < 2) {
        g_last_error = "授权失败：无法获取足够的机器硬件标识。";
        return std::string();
    }

    /*
     * 构造固定格式机器码原始串。
     */
    const std::string raw_machine_string =
        build_machine_raw_string(cpu, disk, board);

    /*
     * 对机器码原始串做 SHA256。
     *
     * sha256() 返回 32 字节二进制摘要。
     */
    const bhc::util::Bytes digest =
        bhc::util::sha256(raw_machine_string);

    /*
     * 将二进制摘要编码为人工友好字符。
     *
     * 机器码主体取 8 位。
     *
     * 最终显示为：
     *
     *     BHC-XXXX-XXXX
     */
    const std::string body = encode_human_base32(digest, 8);

    /*
     * 格式化机器码。
     */
    g_cached_machine_code = format_machine_code(body);

    return g_cached_machine_code;
}


/*
 * build_serial_message()
 *
 * 功能：
 *
 *     构造用于 HMAC-SHA256 的注册码消息。
 *
 * 参数：
 *
 *     machine_code：
 *
 *         当前机器码，例如：
 *
 *             BHC-7D92-F3A1
 *
 * 返回值：
 *
 *     HMAC 消息字符串。
 *
 * 为什么不直接只用 machine_code？
 *
 *     加入 PRODUCT_ID 和 SERIAL_VERSION 可以防止：
 *
 *         1. 不同产品注册码混用；
 *         2. 后续算法版本混淆。
 */
std::string build_serial_message(const std::string& machine_code)
{
    std::ostringstream oss;

    oss << "PRODUCT=" << PRODUCT_ID
        << "|SVER=" << SERIAL_VERSION
        << "|MACHINE=" << machine_code;

    return oss.str();
}


/*
 * generate_expected_serial_normalized()
 *
 * 功能：
 *
 *     根据机器码生成理论注册码。
 *
 * 参数：
 *
 *     machine_code：
 *
 *         当前机器码。
 *
 * 返回值：
 *
 *     12 位无横杠注册码主体。
 *
 *     例如：
 *
 *         K8D49F2Q3M7X
 *
 * 注意：
 *
 *     该函数生成的是规范化形式，不带横杠。
 *
 *     显示给客户时可以用 format_serial() 转成：
 *
 *         K8D4-9F2Q-3M7X
 */
std::string generate_expected_serial_normalized(
    const std::string& machine_code)
{
    /*
     * 构造 HMAC 消息。
     */
    const std::string message = build_serial_message(machine_code);

    /*
     * 计算 HMAC-SHA256。
     */
    const bhc::util::Bytes mac =
        bhc::util::hmac_sha256(SERIAL_SECRET, message);

    /*
     * 将 HMAC 结果编码成 12 位人工友好字符。
     */
    return encode_human_base32(mac, 12);
}


/*
 * load_serial_from_file_unlocked()
 *
 * 功能：
 *
 *     从 g_serial_file 指定的文件中读取注册码。
 *
 * 返回值：
 *
 *     成功：
 *
 *         返回规范化后的注册码。
 *
 *     失败：
 *
 *         返回空字符串，并设置 g_last_error。
 *
 * 为什么带 unlocked？
 *
 *     和 generate_machine_code_unlocked() 一样，
 *     这个函数不加锁，由外层调用者保证线程安全。
 */
std::string load_serial_from_file_unlocked()
{
    std::string content;

    /*
     * 尝试读取 license.key 文件。
     */
    if (!read_text_file(g_serial_file, content)) {
        g_last_error = "授权失败：未找到注册码文件： " + g_serial_file;
        return std::string();
    }

    /*
     * 从文件内容中提取注册码。
     */
    const std::string serial_raw = parse_serial_file_content(content);

    if (serial_raw.empty()) {
        g_last_error = "授权失败：注册码文件为空，或文件中未找到注册码。";
        return std::string();
    }

    /*
     * 规范化注册码。
     */
    const std::string serial_normalized = normalize_serial(serial_raw);

    /*
     * 检查格式。
     */
    if (!is_valid_serial_format(serial_normalized)) {
        g_last_error = "授权失败：注册码格式错误。";
        return std::string();
    }

    return serial_normalized;
}


/*
 * invalidate_check_cache_unlocked()
 *
 * 功能：
 *
 *     清除授权校验缓存。
 *
 * 这个函数不清除：
 *
 *     1. g_serial_normalized；
 *     2. g_serial_file。
 *
 * 原因：
 *
 *     set_serial() 后仍然需要保留新注册码；
 *     set_serial_file() 后仍然需要保留新文件路径。
 *
 * 它只清除：
 *
 *     1. g_checked；
 *     2. g_valid；
 *     3. g_cached_machine_code；
 *     4. g_last_error。
 */
void invalidate_check_cache_unlocked()
{
    g_checked = false;
    g_valid = false;
    g_cached_machine_code.clear();
    g_last_error.clear();
}

} // anonymous namespace


std::string get_machine_code()
{
    /*
     * 加锁保护全局缓存和错误信息。
     */
    std::lock_guard<std::mutex> lock(g_mutex);

    /*
     * 调用内部无锁版本。
     */
    return generate_machine_code_unlocked();
}


bool set_serial(const std::string& serial)
{
    /*
     * 加锁保护 g_serial_normalized。
     */
    std::lock_guard<std::mutex> lock(g_mutex);

    /*
     * 先规范化用户输入。
     */
    const std::string normalized = normalize_serial(serial);

    /*
     * 检查注册码格式。
     *
     * 如果格式错误，不保存。
     */
    if (!is_valid_serial_format(normalized)) {
        g_last_error = "授权失败：注册码格式错误.";
        return false;
    }

    /*
     * 保存规范化后的注册码。
     */
    g_serial_normalized = normalized;

    /*
     * 注册码发生变化，之前的校验结果失效。
     */
    invalidate_check_cache_unlocked();

    return true;
}


bool set_serial_file(const std::string& path)
{
    /*
     * 加锁保护 g_serial_file。
     */
    std::lock_guard<std::mutex> lock(g_mutex);

    /*
     * 文件路径不能为空。
     */
    if (trim(path).empty()) {
        g_last_error = "授权失败：注册码文件路径为空。";
        return false;
    }

    /*
     * 保存新的 license.key 路径。
     */
    g_serial_file = trim(path);

    /*
     * 文件路径变化后，之前校验结果失效。
     */
    invalidate_check_cache_unlocked();

    return true;
}


bool save_serial_file(const std::string& path)
{
    /*
     * 加锁保护 g_serial_normalized。
     */
    std::lock_guard<std::mutex> lock(g_mutex);

    /*
     * 路径不能为空。
     */
    if (trim(path).empty()) {
        g_last_error = "授权失败：注册码文件路径为空。";
        return false;
    }

    /*
     * 必须先有一个格式正确的注册码。
     */
    if (!is_valid_serial_format(g_serial_normalized)) {
        g_last_error = "授权失败：注册码格式错误。";
        return false;
    }

    /*
     * 写入文件时使用带横杠格式，方便人工查看。
     */
    const std::string text = format_serial(g_serial_normalized) + "\n";

    /*
     * 写入文件。
     */
    if (!write_text_file(path, text)) {
        g_last_error = "Failed to write serial file: " + path;
        return false;
    }

    return true;
}


bool check()
{
    /*
     * 加锁保证校验状态一致。
     */
    std::lock_guard<std::mutex> lock(g_mutex);

    /*
     * 如果已经校验过，直接返回缓存结果。
     *
     * 这样 bhc::run() 多次调用时不会重复读取硬件信息。
     */
    if (g_checked) {
        return g_valid;
    }

    /*
     * 先默认本次校验已经执行。
     *
     * 后续无论成功失败，g_checked 都为 true。
     */
    g_checked = true;

    /*
     * 默认校验失败。
     *
     * 只有最后全部通过时，才设置 g_valid = true。
     */
    g_valid = false;

    /*
     * 清空旧错误信息。
     */
    g_last_error.clear();

    /*
     * 生成当前机器码。
     */
    const std::string machine_code = generate_machine_code_unlocked();

    /*
     * 如果机器码为空，说明硬件 ID 不足或主板 UUID 无效。
     */
    if (machine_code.empty()) {
        /*
         * generate_machine_code_unlocked() 内部已经设置了 g_last_error。
         */
        if (g_last_error.empty()) {
            g_last_error = "授权失败：无法生成本机机器码。";
        }

        return false;
    }

    /*
     * serial_to_check 是本次要校验的注册码。
     *
     * 优先级：
     *
     *     1. 内存中通过 set_serial() 设置的注册码；
     *     2. license.key 文件中的注册码。
     */
    std::string serial_to_check = g_serial_normalized;

    /*
     * 如果内存中没有注册码，则尝试从文件读取。
     */
    if (serial_to_check.empty()) {
        serial_to_check = load_serial_from_file_unlocked();

        /*
         * 如果文件读取失败或格式错误，直接失败。
         */
        if (serial_to_check.empty()) {
            if (g_last_error.empty()) {
                g_last_error = "授权失败：未检测到注册码。";
            }

            return false;
        }
    }

    /*
     * 再次检查注册码格式。
     *
     * 这是防御性判断。
     *
     * 正常情况下：
     *
     *     set_serial() 和 load_serial_from_file_unlocked()
     *     已经检查过格式。
     */
    if (!is_valid_serial_format(serial_to_check)) {
        g_last_error = "授权失败：注册码格式错误，应为 XXXX-XXXX-XXXX 格式。";
        return false;
    }

    /*
     * 根据当前机器码计算理论注册码。
     */
    const std::string expected_serial =
        generate_expected_serial_normalized(machine_code);

    /*
     * 比较用户注册码和理论注册码。
     *
     * 两者都是规范化后的 12 位字符串，不包含横杠。
     */
    if (serial_to_check != expected_serial) {
        g_last_error =
            "授权失败：注册码与当前机器不匹配。本机机器码： " +
            machine_code;
        return false;
    }

    /*
     * 到这里说明：
     *
     *     1. 机器码生成成功；
     *     2. 注册码存在；
     *     3. 注册码格式正确；
     *     4. 注册码和当前机器码匹配。
     *
     * 授权通过。
     */
    g_valid = true;
    g_last_error.clear();

    return true;
}


std::string last_error()
{
    /*
     * 加锁读取错误信息。
     */
    std::lock_guard<std::mutex> lock(g_mutex);

    return g_last_error;
}


void clear_cache()
{
    /*
     * 加锁清除缓存。
     */
    std::lock_guard<std::mutex> lock(g_mutex);

    /*
     * 只清除校验缓存，不清除已设置的注册码和文件路径。
     */
    invalidate_check_cache_unlocked();
}

} // namespace license
} // namespace bhc