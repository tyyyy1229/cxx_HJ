/*
 * bhc_license_tool.cpp
 *
 * BellhopCXX 客户侧授权工具。
 *
 * 该工具用于替代原来的 bhc_machine_code 工具。
 *
 * 原来的 bhc_machine_code 只负责显示机器码。
 *
 * 现在这个 bhc_license_tool 负责完整的客户侧离线激活流程：
 *
 *     1. 检查 license.key 是否存在；
 *     2. 如果 license.key 存在并且有效，提示“当前授权有效”；
 *     3. 如果 license.key 不存在或无效，显示本机机器码；
 *     4. 客户把机器码发给供应商；
 *     5. 供应商用内部注册机生成注册码；
 *     6. 客户在本工具中输入注册码；
 *     7. 工具校验注册码；
 *     8. 校验通过后自动生成 license.key；
 *     9. 后续主程序运行时只需要读取 license.key 并执行 check()。
 *
 * 注意：
 *
 *     这个工具可以交付给客户。
 *
 *     这个工具不能生成注册码。
 *
 *     这个工具不包含注册机逻辑。
 *
 *     这个工具不输出 CPU、硬盘、主板等原始硬件信息。
 *
 *     这个工具只输出最终机器码：
 *
 *         BHC-XXXX-XXXX
 *
 * 主程序中的职责：
 *
 *     bellhopParam::runMod()
 *         只调用 bhc::license::check()
 *
 *     bhc::run()
 *         只调用 bhc::license::check()
 *
 *     主程序不负责 std::cin 输入注册码。
 *
 *     首次激活统一由本工具完成。
 */

#include <bhc/license.hpp>

#include <iostream>
#include <string>


/*
 * DEFAULT_LICENSE_FILE
 *
 * 默认授权文件名。
 *
 * 该文件由本工具自动创建。
 *
 * 主程序运行时，license.cpp 默认也会读取当前工作目录下的：
 *
 *     license.key
 *
 * 所以客户通常应把 license.key 放在主程序运行目录。
 *
 * 如果客户希望把授权文件放在其他路径，可以运行：
 *
 *     ./bhc_license_tool /path/to/license.key
 *
 * 这样工具会使用指定路径。
 */
static const char* DEFAULT_LICENSE_FILE = "license.key";


/*
 * print_line()
 *
 * 功能：
 *
 *     输出一条分隔线。
 *
 * 作用：
 *
 *     1. 提高命令行显示可读性；
 *     2. 让客户现场人员更容易区分不同信息块；
 *     3. 后续如果想修改分隔线样式，只需要改这里。
 */
static void print_line()
{
    std::cout << "----------------------------------------" << std::endl;
}


/*
 * print_title()
 *
 * 功能：
 *
 *     输出工具标题。
 *
 * 这个函数只负责显示，不参与授权逻辑。
 */
static void print_title()
{
    print_line();
    std::cout << "BellhopCXX 客户授权工具" << std::endl;
    print_line();
}


/*
 * print_usage()
 *
 * 功能：
 *
 *     输出工具用法。
 *
 * 参数：
 *
 *     program_name：
 *
 *         当前程序名，一般来自 argv[0]。
 *
 * 支持的调用方式：
 *
 *     1. 不带参数：
 *
 *            ./bhc_license_tool
 *
 *        使用默认授权文件：
 *
 *            license.key
 *
 *     2. 带一个参数：
 *
 *            ./bhc_license_tool ./config/license.key
 *
 *        使用用户指定的授权文件路径。
 */
static void print_usage(const char* program_name)
{
    std::cout << "用法：" << std::endl;
    std::cout << "  " << program_name << std::endl;
    std::cout << "  " << program_name << " <授权文件路径>" << std::endl;
    std::cout << std::endl;

    std::cout << "示例：" << std::endl;
    std::cout << "  " << program_name << std::endl;
    std::cout << "  " << program_name << " license.key" << std::endl;
    std::cout << "  " << program_name << " ./config/license.key" << std::endl;
    std::cout << std::endl;

    std::cout << "说明：" << std::endl;
    std::cout << "  1. 不指定路径时，默认使用当前目录下的 license.key。" << std::endl;
    std::cout << "  2. 本工具只负责客户侧激活，不负责生成注册码。" << std::endl;
    std::cout << "  3. 注册码需要由供应商根据本机机器码生成。" << std::endl;
}


/*
 * print_activation_note()
 *
 * 功能：
 *
 *     输出首次激活说明。
 *
 * 为什么需要？
 *
 *     客户现场人员可能不清楚机器码和注册码的关系。
 *
 *     这里明确告诉客户：
 *
 *         机器码不是注册码；
 *         需要把机器码发给供应商；
 *         供应商返回注册码后再输入。
 */
static void print_activation_note()
{
    std::cout << "激活说明：" << std::endl;
    std::cout << "  1. 本机机器码用于向供应商申请注册码。" << std::endl;
    std::cout << "  2. 机器码不是注册码，不能直接作为注册码输入。" << std::endl;
    std::cout << "  3. 请输入供应商返回的注册码。" << std::endl;
    std::cout << "  4. 注册码校验成功后，本工具会自动生成 license.key。" << std::endl;
}


/*
 * read_serial_from_stdin()
 *
 * 功能：
 *
 *     从命令行读取客户输入的注册码。
 *
 * 返回值：
 *
 *     用户输入的原始注册码字符串。
 *
 * 注意：
 *
 *     这里不负责判断注册码格式。
 *
 *     格式判断交给：
 *
 *         bhc::license::set_serial()
 *
 *     因为 set_serial() 内部会统一完成：
 *
 *         1. 去掉横杠；
 *         2. 去掉空格；
 *         3. 转大写；
 *         4. 检查字符；
 *         5. 检查长度。
 */
static std::string read_serial_from_stdin()
{
    std::cout << "请输入注册码：";

    /*
     * serial 保存用户输入。
     *
     * 因为注册码推荐格式是：
     *
     *     XXXX-XXXX-XXXX
     *
     * 中间没有空格，所以使用 operator>> 即可。
     */
    std::string serial;
    std::cin >> serial;

    return serial;
}


/*
 * print_last_license_error()
 *
 * 功能：
 *
 *     输出授权模块中的最后一次错误信息。
 *
 * 参数：
 *
 *     prefix：
 *
 *         错误信息前缀。
 *
 *         例如：
 *
 *             "原因："
 *             "保存失败："
 *
 * 设计原因：
 *
 *     多处都会需要输出：
 *
 *         bhc::license::last_error()
 *
 *     抽成函数可以减少重复代码。
 */
static void print_last_license_error(const std::string& prefix)
{
    /*
     * 从授权模块获取最近一次错误信息。
     */
    const std::string error = bhc::license::last_error();

    /*
     * 如果错误信息非空，则输出。
     */
    if (!error.empty()) {
        std::cout << prefix << error << std::endl;
    }
}


/*
 * main()
 *
 * 功能：
 *
 *     客户授权工具入口。
 *
 * 命令行参数：
 *
 *     argc：
 *
 *         参数数量。
 *
 *     argv：
 *
 *         参数数组。
 *
 * 支持形式：
 *
 *     1. ./bhc_license_tool
 *
 *        使用默认 license.key。
 *
 *     2. ./bhc_license_tool ./config/license.key
 *
 *        使用指定授权文件。
 *
 * 返回值：
 *
 *     0：
 *
 *         授权已经有效，或者激活成功并保存 license.key。
 *
 *     1：
 *
 *         参数错误、机器码生成失败、注册码错误、授权文件保存失败等。
 */
int main(int argc, char* argv[])
{
    /*
     * 输出标题。
     */
    print_title();

    /*
     * 参数数量检查。
     *
     * argc == 1：
     *
     *     只有程序名，没有额外参数。
     *     使用默认 license.key。
     *
     * argc == 2：
     *
     *     argv[1] 作为授权文件路径。
     *
     * 其他情况：
     *
     *     参数错误，输出用法并退出。
     */
    if (argc > 2) {
        std::cout << "参数错误。" << std::endl;
        std::cout << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    /*
     * license_file 保存本次使用的授权文件路径。
     *
     * 默认值是：
     *
     *     license.key
     *
     * 如果用户传入路径，则使用用户传入路径。
     */
    const std::string license_file =
        (argc == 2) ? std::string(argv[1]) : std::string(DEFAULT_LICENSE_FILE);

    /*
     * 告诉授权模块本次使用哪个授权文件。
     *
     * 后续 bhc::license::check() 会尝试从这个路径读取注册码。
     */
    if (!bhc::license::set_serial_file(license_file)) {
        std::cout << "授权文件路径设置失败。" << std::endl;
        print_last_license_error("原因：");
        return 1;
    }

    /*
     * 第一次授权检查。
     *
     * 如果 license.key 已经存在并且有效，直接成功退出。
     *
     * 这种情况下：
     *
     *     1. 不显示机器码；
     *     2. 不要求输入注册码；
     *     3. 不覆盖 license.key。
     */
    if (bhc::license::check()) {
        std::cout << "当前授权有效。" << std::endl;
        std::cout << "授权文件：" << license_file << std::endl;
        print_line();
        return 0;
    }

/*
 * 如果走到这里，说明当前授权未通过。
 *
 * 可能原因：
 *
 *     1. license.key 不存在；
 *     2. license.key 为空；
 *     3. license.key 中的注册码格式错误；
 *     4. license.key 中的注册码与当前机器不匹配。
 *
 * 这里先输出原因。
 */
std::cout << "未检测到有效授权。" << std::endl;
print_last_license_error("原因：");

/*
 * 授权无效时，必须先生成并显示本机机器码。
 *
 * 客户需要把这个机器码提供给供应商，
 * 供应商再用内部注册机生成对应注册码。
 */
const std::string machine_code = bhc::license::get_machine_code();

/*
 * 如果机器码为空，说明无法获取足够的硬件标识。
 *
 * 这种情况下客户无法申请注册码，所以不能继续进入输入注册码流程。
 */
if (machine_code.empty()) {
    std::cout << "无法生成本机机器码，无法继续激活。" << std::endl;
    print_last_license_error("原因：");
    print_line();
    return 1;
}

/*
 * 这里必须在“请输入注册码”之前输出机器码。
 */
std::cout << std::endl;
std::cout << "本机机器码：" << machine_code << std::endl;
std::cout << "请将该机器码提供给供应商获取注册码。" << std::endl;
std::cout << std::endl;

/*
 * 输出激活说明。
 */
print_activation_note();

std::cout << std::endl;

/*
 * 循环读取并校验注册码。
 *
 * 如果输入错误，不退出程序，
 * 而是重新回到“请输入注册码：”。
 */
while (true) {
    const std::string serial = read_serial_from_stdin();

    if (!bhc::license::set_serial(serial)) {
        std::cout << "注册码格式错误，请重新输入。" << std::endl;
        print_last_license_error("原因：");
        std::cout << std::endl;
        continue;
    }

    if (!bhc::license::check()) {
        std::cout << "注册码校验失败，请重新输入。" << std::endl;
        print_last_license_error("原因：");
        std::cout << std::endl;
        continue;
    }

    break;
}

    /*
     * 走到这里说明注册码校验通过。
     *
     * 现在将注册码保存到 license.key。
     *
     * 保存成功后，主程序后续运行时只需要调用 check()，
     * 不需要再输入注册码。
     */
    if (!bhc::license::save_serial_file(license_file)) {
        std::cout << "授权校验已通过，但授权文件保存失败。" << std::endl;
        print_last_license_error("原因：");
        std::cout << "请检查目录权限或手动创建授权文件。" << std::endl;
        print_line();

        /*
         * 这里返回 1。
         *
         * 原因：
         *
         *     本工具的目标是完成激活并创建 license.key。
         *     如果保存失败，虽然本次校验通过，但激活流程没有完整完成。
         */
        return 1;
    }

    /*
     * 激活完成。
     */
    std::cout << std::endl;
    std::cout << "授权成功，已自动生成授权文件：" << license_file << std::endl;
    std::cout << "后续运行主程序时，将自动读取该授权文件。" << std::endl;
    print_line();

    return 0;
}