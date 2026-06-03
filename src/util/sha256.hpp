#pragma once

/*
 * sha256.hpp
 *
 * 该文件是 BellhopCXX 授权模块使用的 SHA256 / HMAC-SHA256 工具头文件。
 *
 * 设计目的：
 *
 *     1. 不依赖 OpenSSL、libsodium 等外部库；
 *     2. 便于静态库交付；
 *     3. 便于在涉密机房等离线环境中部署；
 *     4. 为 license.cpp 提供机器码 Hash 和注册码校验能力。
 *
 * 本文件只负责声明函数，不负责实现算法。
 *
 * 对应实现文件：
 *
 *     src/util/sha256.cpp
 *
 * 在授权模块中的用途：
 *
 *     1. 机器码生成：
 *
 *         CPU_ID + DISK_SERIAL + BOARD_UUID
 *              ↓
 *         固定格式拼接
 *              ↓
 *         SHA256
 *              ↓
 *         截断并格式化为 BHC-XXXX-XXXX
 *
 *     2. 注册码生成 / 校验：
 *
 *         machine_code + product_id + secret
 *              ↓
 *         HMAC-SHA256
 *              ↓
 *         截断并格式化为 XXXX-XXXX-XXXX
 *
 * 注意：
 *
 *     SHA256 是摘要算法，不是加密算法。
 *
 *     它的作用是把任意长度输入转换成固定长度摘要。
 *
 *     HMAC-SHA256 是带密钥的摘要算法。
 *
 *     它的作用是：
 *
 *         在不知道 secret 的情况下，外部人员很难根据机器码伪造正确注册码。
 */

#include <cstdint>
#include <string>
#include <vector>

/*
 * 当前工具函数放在 bhc::util 命名空间中。
 *
 * 命名空间设计原因：
 *
 *     1. bhc 表示 BellhopCXX 项目；
 *     2. util 表示通用工具模块；
 *     3. 避免 sha256_hex、hmac_sha256_hex 等函数污染全局命名空间；
 *     4. 后续如果还有 base32、fileutil 等工具，也可以继续放在 bhc::util 下。
 */
namespace bhc {
namespace util {

/*
 * Bytes 类型别名。
 *
 * std::vector<std::uint8_t> 表示一段二进制字节数据。
 *
 * 为什么需要这个别名？
 *
 *     SHA256 的底层算法处理的是“字节序列”，而不是 C++ 字符串。
 *
 *     例如：
 *
 *         字符串 "ABC"
 *
 *     实际参与 SHA256 运算的是三个字节：
 *
 *         0x41 0x42 0x43
 *
 *     使用 Bytes 可以让函数接口表达得更清楚。
 *
 * std::uint8_t 说明：
 *
 *     std::uint8_t 是 C++ 标准库中的 8 位无符号整数类型。
 *
 *     一个 std::uint8_t 正好表示一个字节。
 */
using Bytes = std::vector<std::uint8_t>;


/*
 * sha256()
 *
 * 功能：
 *
 *     对一段二进制字节数据计算 SHA256 摘要。
 *
 * 参数：
 *
 *     data：
 *
 *         输入数据。
 *
 *         类型是 Bytes，也就是 std::vector<std::uint8_t>。
 *
 *         data 中的每个元素表示一个字节。
 *
 * 返回值：
 *
 *     返回 SHA256 摘要的原始二进制形式。
 *
 *     SHA256 的输出固定为 32 字节，也就是 256 bit。
 *
 *     因此返回值长度应该始终是 32。
 *
 * 使用场景：
 *
 *     当调用方已经拥有二进制数据时，使用这个函数。
 *
 *     例如：
 *
 *         Bytes digest = sha256(raw_bytes);
 *
 * 注意：
 *
 *     这个函数返回的是二进制字节，不是十六进制字符串。
 *
 *     如果需要得到可打印的字符串形式，应使用 sha256_hex()。
 */
Bytes sha256(const Bytes& data);


/*
 * sha256()
 *
 * 功能：
 *
 *     对 std::string 类型输入计算 SHA256 摘要。
 *
 * 参数：
 *
 *     text：
 *
 *         输入字符串。
 *
 *         函数内部会把字符串中的每个字符按字节处理。
 *
 *         例如：
 *
 *             "PRODUCT=BHC2D|MCVER=MC1|CPU=..."
 *
 * 返回值：
 *
 *     返回 SHA256 摘要的原始二进制形式。
 *
 *     长度固定为 32 字节。
 *
 * 使用场景：
 *
 *     授权模块中机器码原始拼接串是 std::string，
 *     所以 license.cpp 中通常会调用这个重载版本。
 *
 * 示例：
 *
 *     Bytes digest = sha256(raw_machine_string);
 */
Bytes sha256(const std::string& text);


/*
 * sha256_hex()
 *
 * 功能：
 *
 *     对字符串计算 SHA256，并返回十六进制字符串。
 *
 * 参数：
 *
 *     text：
 *
 *         输入字符串。
 *
 * 返回值：
 *
 *     返回 64 个字符长度的十六进制字符串。
 *
 *     原因：
 *
 *         SHA256 输出 32 字节；
 *
 *         每个字节用两个十六进制字符表示；
 *
 *         所以总长度为：
 *
 *             32 * 2 = 64
 *
 * 示例：
 *
 *     std::string hash = sha256_hex("ABC");
 *
 *     返回类似：
 *
 *         b5d4045c3f466fa91fe2cc6abe79232a...
 *
 * 在授权模块中的用途：
 *
 *     可以用于：
 *
 *         1. 调试机器码原始 Hash；
 *         2. 直接截断前若干位生成短机器码；
 *         3. 作为 HMAC 内部辅助。
 *
 * 注意：
 *
 *     该函数返回的是小写十六进制字符串。
 *
 *     如果机器码需要大写，可以在 license.cpp 中统一转大写。
 */
std::string sha256_hex(const std::string& text);


/*
 * hmac_sha256()
 *
 * 功能：
 *
 *     使用 HMAC-SHA256 对一段字符串消息进行带密钥摘要计算。
 *
 * 参数：
 *
 *     key：
 *
 *         HMAC 使用的密钥。
 *
 *         在授权模块中，它相当于内部私有种子 secret。
 *
 *         注意：
 *
 *             如果采用 HMAC 短注册码方案，key 会被编译进静态库。
 *
 *             因此它不能达到非对称签名那种强安全性，
 *             但对于“防普通复制、防误用、防随意扩散”的场景是可用的。
 *
 *     message：
 *
 *         要计算 HMAC 的消息内容。
 *
 *         在授权模块中通常可以是：
 *
 *             PRODUCT=BHC2D|SERIALVER=S1|MACHINE=BHC-7D92-F3A1
 *
 * 返回值：
 *
 *     返回 HMAC-SHA256 的原始二进制结果。
 *
 *     长度固定为 32 字节。
 *
 * 使用场景：
 *
 *     license.cpp 中根据机器码计算理论注册码：
 *
 *         Bytes mac = hmac_sha256(secret, message);
 *
 *     然后再把 mac 编码、截断、分组，生成：
 *
 *         XXXX-XXXX-XXXX
 */
Bytes hmac_sha256(const std::string& key, const std::string& message);


/*
 * hmac_sha256_hex()
 *
 * 功能：
 *
 *     使用 HMAC-SHA256 对字符串消息进行计算，
 *     并将结果转换成十六进制字符串。
 *
 * 参数：
 *
 *     key：
 *
 *         HMAC 密钥。
 *
 *         在授权模块中对应产品私有 secret。
 *
 *     message：
 *
 *         HMAC 消息。
 *
 *         在授权模块中通常包含：
 *
 *             产品编号
 *             注册码版本
 *             机器码
 *
 * 返回值：
 *
 *     返回 64 个字符长度的十六进制字符串。
 *
 * 在授权模块中的用途：
 *
 *     可以直接截断十六进制字符串生成短注册码。
 *
 *     例如：
 *
 *         std::string mac = hmac_sha256_hex(secret, message);
 *         std::string serial_body = mac.substr(0, 12);
 *
 *     然后格式化为：
 *
 *         XXXX-XXXX-XXXX
 *
 * 注意：
 *
 *     第一版如果想实现简单，可以直接用 hex 前 12 位作为注册码主体。
 *
 *     如果后续想减少字符混淆，可以再增加 Base32 编码函数。
 */
std::string hmac_sha256_hex(const std::string& key, const std::string& message);


/*
 * bytes_to_hex()
 *
 * 功能：
 *
 *     将二进制字节数组转换成十六进制字符串。
 *
 * 参数：
 *
 *     bytes：
 *
 *         输入字节数组。
 *
 * 返回值：
 *
 *     返回十六进制字符串。
 *
 * 示例：
 *
 *     输入字节：
 *
 *         0xAB 0xCD 0x01
 *
 *     输出字符串：
 *
 *         "abcd01"
 *
 * 用途：
 *
 *     SHA256 和 HMAC-SHA256 的原始结果都是二进制字节。
 *
 *     为了显示、截断、保存或调试，需要转换为字符串。
 */
std::string bytes_to_hex(const Bytes& bytes);


/*
 * hex_to_bytes()
 *
 * 功能：
 *
 *     将十六进制字符串转换回二进制字节数组。
 *
 * 参数：
 *
 *     hex：
 *
 *         十六进制字符串。
 *
 *         可以是大写，也可以是小写。
 *
 *         例如：
 *
 *             "ABCD01"
 *             "abcd01"
 *
 * 返回值：
 *
 *     转换后的字节数组。
 *
 * 注意：
 *
 *     如果 hex 长度不是偶数，或者包含非法十六进制字符，
 *     实现文件中可以选择：
 *
 *         1. 返回空数组；
 *         2. 或者忽略非法输入；
 *         3. 或者抛异常。
 *
 *     为了授权模块稳定，建议第一版返回空数组，不抛异常。
 *
 * 当前授权模块第一版主要需要 bytes_to_hex()，
 * hex_to_bytes() 是为了后续扩展预留。
 */
Bytes hex_to_bytes(const std::string& hex);

} // namespace util
} // namespace bhc