#pragma once

/*
 * license.hpp
 *
 * 该文件是 BellhopCXX 静态库授权模块的对外接口头文件。
 *
 * 它只负责声明授权相关函数，不负责实现具体逻辑。
 *
 * 具体实现应放在：
 *
 *     src/license.cpp
 *
 * 该授权模块的第一版设计目标是：
 *
 *     1. 离线授权；
 *     2. 一机一码；
 *     3. 不依赖网络；
 *     4. 不使用复杂 license 文件；
 *     5. 支持涉密机房中人工抄写短机器码和短注册码；
 *     6. 客户只需要输入类似 XXXX-XXXX-XXXX 的短注册码。
 *
 * 机器码生成逻辑由实现文件负责，大致流程为：
 *
 *     CPU_ID + DISK_SERIAL + BOARD_UUID
 *          ↓
 *     规范化，例如转大写、去空格、去横杠
 *          ↓
 *     过滤无效硬件 ID
 *          ↓
 *     按固定格式拼接
 *          ↓
 *     SHA256 / HMAC 等摘要算法
 *          ↓
 *     截断并格式化为 BHC-XXXX-XXXX
 *
 * 注意：
 *
 *     本头文件不暴露 CPU ID、硬盘序列号、主板 UUID 等原始硬件信息。
 *     对外只暴露最终生成的短机器码。
 */

#include <string>

/*
 * BHC_API 宏说明
 *
 * 这个宏用于控制函数导出。
 *
 * 当前项目主要交付静态库，因此大多数情况下 BHC_API 可以为空。
 *
 * 但是为了兼容未来可能构建动态库的情况，这里保留 Windows 下的
 * __declspec(dllexport) / __declspec(dllimport) 处理。
 *
 * 为什么这里要单独定义 BHC_API？
 *
 *     因为 license.hpp 需要允许客户单独包含：
 *
 *         #include <bhc/license.hpp>
 *
 *     如果强制依赖 bhc.hpp 或 platform.hpp，可能会受到原有工程
 *     头文件包含顺序的限制。
 *
 * 判断逻辑：
 *
 *     1. 如果外部已经定义过 BHC_API，则直接使用外部定义；
 *     2. 如果当前是 Windows 平台，则根据 BHC_DLL_EXPORT /
 *        BHC_DLL_IMPORT 判断是导出还是导入；
 *     3. 如果是 Linux / macOS / 静态库场景，则 BHC_API 为空。
 */
#ifndef BHC_API

    /*
     * _WIN32 是 MSVC、MinGW 等 Windows 编译环境常见的预定义宏。
     *
     * 只要在 Windows 下编译，一般都会定义 _WIN32。
     */
    #ifdef _WIN32

        /*
         * BHC_DLL_EXPORT：
         *
         *     表示当前正在编译 BellhopCXX 动态库本体。
         *
         *     如果以后你把当前项目编译成 DLL，那么在编译 DLL 时
         *     可以定义 BHC_DLL_EXPORT，使函数符号导出。
         */
        #if defined(BHC_DLL_EXPORT)

            /*
             * __declspec(dllexport)：
             *
             *     Windows DLL 导出符号说明。
             *
             *     含义是：把该函数导出到 DLL 外部，使客户程序可以调用。
             */
            #define BHC_API __declspec(dllexport)

        /*
         * BHC_DLL_IMPORT：
         *
         *     表示当前代码正在使用 BellhopCXX 动态库。
         *
         *     如果客户程序链接 DLL 版本，可以定义 BHC_DLL_IMPORT。
         */
        #elif defined(BHC_DLL_IMPORT)

            /*
             * __declspec(dllimport)：
             *
             *     Windows DLL 导入符号说明。
             *
             *     含义是：该函数来自外部 DLL。
             */
            #define BHC_API __declspec(dllimport)

        #else

            /*
             * 既没有定义 BHC_DLL_EXPORT，也没有定义 BHC_DLL_IMPORT。
             *
             * 这种情况通常对应：
             *
             *     1. 静态库构建；
             *     2. 普通可执行程序内部使用；
             *     3. 不需要显式导入导出符号。
             *
             * 因此 BHC_API 置为空。
             */
            #define BHC_API

        #endif

    #else

        /*
         * 非 Windows 平台。
         *
         * Linux 下静态库 / 普通可执行程序一般不需要 __declspec。
         *
         * 如果以后要控制 GCC/Clang 的符号可见性，可以在这里扩展为：
         *
         *     __attribute__((visibility("default")))
         *
         * 第一版暂时保持为空，最简单稳定。
         */
        #define BHC_API

    #endif

#endif


/*
 * 所有授权接口统一放在 bhc::license 命名空间中。
 *
 * 为什么不直接放在 bhc 命名空间？
 *
 *     1. 避免和现有 bhc::run、bhc::setup 等接口混在一起；
 *     2. 授权功能属于独立模块；
 *     3. 后续如果增加 has_feature、get_license_status 等接口，
 *        也更容易管理。
 */
namespace bhc {
namespace license {

/*
 * get_machine_code()
 *
 * 功能：
 *
 *     获取当前机器的短机器码。
 *
 * 机器码来源：
 *
 *     当前版本机器码只使用三个硬件字段：
 *
 *         1. CPU_ID
 *         2. DISK_SERIAL
 *         3. BOARD_UUID
 *
 *     这些字段不会直接返回给客户。
 *
 * 内部实现预期：
 *
 *     src/license.cpp 中应该完成以下工作：
 *
 *         1. 读取 CPU_ID；
 *         2. 读取系统盘序列号 DISK_SERIAL；
 *         3. 读取主板 UUID BOARD_UUID；
 *         4. 对三个字段进行规范化；
 *         5. 过滤 EMPTY、UNKNOWN、00000000 等无效值；
 *         6. 判断是否满足生成机器码的最低条件；
 *         7. 按固定格式拼接原始字符串；
 *         8. 对拼接字符串做 Hash；
 *         9. 截断 Hash；
 *        10. 格式化为 BHC-XXXX-XXXX。
 *
 * 推荐的拼接格式：
 *
 *     PRODUCT=BHC2D|MCVER=MC1|CPU={CPU}|DISK={DISK}|BOARD={BOARD}|SALT=BHC_MACHINE_CODE_2026
 *
 * 返回值：
 *
 *     成功：
 *
 *         返回格式化后的机器码，例如：
 *
 *             BHC-7D92-F3A1
 *
 *     失败：
 *
 *         返回空字符串 ""。
 *
 *         失败原因可以通过 last_error() 获取。
 *
 * 注意：
 *
 *     该函数不应该返回 CPU、硬盘、主板的原始信息。
 *     这样可以减少涉密环境中对硬件信息外泄的顾虑。
 */
BHC_API std::string get_machine_code();


/*
 * set_serial()
 *
 * 功能：
 *
 *     直接设置注册码字符串。
 *
 * 典型使用场景：
 *
 *     1. 客户第一次运行程序；
 *     2. 程序显示机器码；
 *     3. 客户向供应商索取注册码；
 *     4. 客户手动输入注册码；
 *     5. 程序调用 set_serial(serial) 暂存这个注册码；
 *     6. 程序调用 check() 验证注册码是否正确。
 *
 * 参数：
 *
 *     serial：
 *
 *         用户输入的注册码字符串。
 *
 *         例如：
 *
 *             K8D4-9F2Q-3M7X
 *
 *         实现文件中应该对 serial 做规范化处理。
 *
 *         例如下面几种输入应当被视为等价：
 *
 *             K8D4-9F2Q-3M7X
 *             k8d4-9f2q-3m7x
 *             K8D4 9F2Q 3M7X
 *             K8D49F2Q3M7X
 *
 *         也就是说，实现时应当：
 *
 *             1. 去掉空格；
 *             2. 去掉横杠；
 *             3. 转成大写；
 *             4. 检查字符是否合法；
 *             5. 检查长度是否符合预期。
 *
 * 返回值：
 *
 *     true：
 *
 *         输入的注册码字符串被接受。
 *
 *         注意：
 *
 *             true 只表示“格式上可以接受”，
 *             不代表注册码一定正确。
 *
 *             是否真正匹配当前机器，要由 check() 判断。
 *
 *     false：
 *
 *         输入为空，或者格式明显非法。
 *
 *         例如：
 *
 *             1. 空字符串；
 *             2. 包含非法字符；
 *             3. 长度不符合预期。
 *
 * 副作用：
 *
 *     该函数通常应该清除上一次 check() 的缓存结果。
 *
 *     因为注册码发生变化后，之前的校验结果已经不可靠。
 */
BHC_API bool set_serial(const std::string& serial);


/*
 * set_serial_file()
 *
 * 功能：
 *
 *     设置注册码文件路径。
 *
 *     第一版授权方案中，注册码文件建议命名为：
 *
 *         license.key
 *
 *     文件内容尽量简单，只需要一行注册码。
 *
 * 参数：
 *
 *     path：
 *
 *         license.key 文件路径。
 *
 *         例如：
 *
 *             license.key
 *             ./license.key
 *             /opt/bellhop/license.key
 *
 * 支持的文件内容格式：
 *
 *     第一种，只有注册码：
 *
 *         K8D4-9F2Q-3M7X
 *
 *     第二种，键值形式：
 *
 *         SERIAL=K8D4-9F2Q-3M7X
 *
 *     实现文件中可以同时兼容这两种格式。
 *
 * 返回值：
 *
 *     true：
 *
 *         文件路径被接受。
 *
 *         注意：
 *
 *             true 不代表文件一定存在，也不代表注册码正确。
 *
 *             是否成功读取、是否校验通过，由 check() 判断。
 *
 *     false：
 *
 *         path 为空，无法作为有效路径。
 *
 * 副作用：
 *
 *     该函数通常也应该清除上一次 check() 的缓存结果。
 *
 *     因为注册码文件路径改变后，之前的校验结果已经不可靠。
 */
BHC_API bool set_serial_file(const std::string& path);


/*
 * save_serial_file()
 *
 * 功能：
 *
 *     将当前已经设置的注册码保存到指定文件。
 *
 * 典型使用场景：
 *
 *     客户第一次手动输入注册码后：
 *
 *         1. 程序调用 set_serial(serial)；
 *         2. 程序调用 check()；
 *         3. 如果 check() 返回 true；
 *         4. 程序调用 save_serial_file("license.key")；
 *         5. 后续运行时自动读取 license.key，不需要再次输入。
 *
 * 参数：
 *
 *     path：
 *
 *         要保存的文件路径。
 *
 *         通常是：
 *
 *             license.key
 *
 * 文件内容建议：
 *
 *     为了简单，可以只写入一行注册码：
 *
 *         K8D4-9F2Q-3M7X
 *
 *     也可以写成：
 *
 *         SERIAL=K8D4-9F2Q-3M7X
 *
 *     具体格式由 src/license.cpp 实现决定。
 *
 * 返回值：
 *
 *     true：
 *
 *         保存成功。
 *
 *     false：
 *
 *         保存失败。
 *
 *         可能原因包括：
 *
 *             1. 当前没有设置注册码；
 *             2. path 为空；
 *             3. 文件无法创建；
 *             4. 文件没有写权限；
 *             5. 磁盘或目录不可用。
 *
 * 注意：
 *
 *     该函数不负责判断注册码是否正确。
 *
 *     推荐调用顺序是：
 *
 *         set_serial(serial)
 *         check()
 *         save_serial_file("license.key")
 *
 *     不建议在 check() 失败时保存注册码。
 */
BHC_API bool save_serial_file(const std::string& path);


/*
 * check()
 *
 * 功能：
 *
 *     执行授权校验。
 *
 * 这是整个 license 模块最核心的函数。
 *
 * 内部校验流程预期：
 *
 *     1. 生成当前机器的机器码；
 *
 *        调用 get_machine_code() 或内部机器码生成函数。
 *
 *        如果机器码生成失败，则授权失败。
 *
 *     2. 获取当前注册码；
 *
 *        注册码来源可以有两种：
 *
 *            a. set_serial() 已经设置过的内存注册码；
 *            b. set_serial_file() 指定的 license.key 文件。
 *
 *        优先级建议：
 *
 *            内存中的注册码优先；
 *            如果内存中没有，再读取 license.key。
 *
 *     3. 规范化注册码；
 *
 *        例如：
 *
 *            去空格；
 *            去横杠；
 *            转大写；
 *            检查长度；
 *            检查字符集。
 *
 *     4. 根据当前机器码计算理论注册码；
 *
 *        例如：
 *
 *            expected_serial = HMAC_SHA256(secret, machine_code + product_id)
 *
 *        然后截断并格式化。
 *
 *     5. 比较用户注册码和理论注册码；
 *
 *        如果一致，则授权通过。
 *        如果不一致，则授权失败。
 *
 * 返回值：
 *
 *     true：
 *
 *         当前机器码和注册码匹配，允许继续运行。
 *
 *     false：
 *
 *         授权失败。
 *
 *         可能原因包括：
 *
 *             1. 无法生成机器码；
 *             2. 没有找到 license.key；
 *             3. license.key 为空；
 *             4. 注册码格式错误；
 *             5. 注册码和当前机器不匹配；
 *             6. 产品编号不匹配；
 *             7. 内部校验算法失败。
 *
 * 使用位置：
 *
 *     第一版建议至少在两个地方调用：
 *
 *         1. config/interface/bellhopParam.cpp
 *
 *            在 bellhopParam::runMod() 开头调用。
 *
 *         2. src/api.cpp
 *
 *            在 bhc::run() 开头调用。
 *
 *     这样可以形成两层拦截：
 *
 *         外层接口库拦截一次；
 *         核心计算库再拦截一次。
 */
BHC_API bool check();


/*
 * last_error()
 *
 * 功能：
 *
 *     获取最近一次 license 模块产生的错误信息。
 *
 * 典型使用场景：
 *
 *     if (!bhc::license::check()) {
 *         std::cerr << bhc::license::last_error() << std::endl;
 *     }
 *
 * 返回值：
 *
 *     返回一段可读的错误字符串。
 *
 *     例如：
 *
 *         License file not found.
 *         Serial is empty.
 *         Serial format is invalid.
 *         Failed to get board UUID.
 *         Failed to get enough hardware identifiers.
 *         Serial does not match this machine.
 *
 * 注意：
 *
 *     last_error() 只是辅助诊断接口。
 *
 *     它不应该被用于判断授权是否成功。
 *
 *     判断授权成功或失败必须使用 check() 的返回值。
 */
BHC_API std::string last_error();


/*
 * clear_cache()
 *
 * 功能：
 *
 *     清除授权模块内部缓存状态。
 *
 * 为什么需要缓存？
 *
 *     授权校验可能会读取硬件信息、读取文件、计算 Hash。
 *
 *     如果每次 bhc::run() 都完整执行一遍，可能会有额外开销。
 *
 *     因此实现文件中可以缓存上一次校验结果。
 *
 * 例如内部可能有类似状态：
 *
 *     bool g_checked = false;
 *     bool g_valid = false;
 *     std::string g_machine_code;
 *     std::string g_serial;
 *     std::string g_last_error;
 *
 * clear_cache() 的作用：
 *
 *     1. 清除 g_checked；
 *     2. 清除 g_valid；
 *     3. 允许下一次 check() 重新读取 license.key；
 *     4. 允许下一次 check() 重新计算机器码；
 *     5. 避免旧注册码、旧文件路径、旧校验结果继续生效。
 *
 * 典型调用场景：
 *
 *     1. 调用了 set_serial() 之后；
 *     2. 调用了 set_serial_file() 之后；
 *     3. 客户替换 license.key 之后；
 *     4. 测试授权模块时需要强制重新校验。
 *
 * 注意：
 *
 *     set_serial() 和 set_serial_file() 的实现内部也可以自动调用 clear_cache()。
 *
 *     因此普通客户程序一般不需要主动调用 clear_cache()。
 */
BHC_API void clear_cache();

} // namespace license
} // namespace bhc