/*
 * sha256.cpp
 *
 * 该文件实现 sha256.hpp 中声明的 SHA256 / HMAC-SHA256 工具函数。
 *
 * 设计目标：
 *
 *     1. 不依赖 OpenSSL；
 *     2. 不依赖 libsodium；
 *     3. 不依赖系统加密库；
 *     4. 可以直接编译进 bellhopcxxstatic 静态库；
 *     5. 适合离线涉密环境部署；
 *     6. 为授权模块 license.cpp 提供机器码 Hash 和注册码 HMAC 校验能力。
 *
 * 使用场景：
 *
 *     1. 机器码生成：
 *
 *        raw_machine_string
 *              ↓
 *        SHA256
 *              ↓
 *        截断生成 BHC-XXXX-XXXX
 *
 *     2. 注册码校验：
 *
 *        secret + machine_code
 *              ↓
 *        HMAC-SHA256
 *              ↓
 *        截断生成 XXXX-XXXX-XXXX
 *
 * 注意：
 *
 *     SHA256 是摘要算法，不是加密算法。
 *
 *     它不能把数据“加密后再解密”，只能把任意长度输入转换为固定长度摘要。
 *
 *     HMAC-SHA256 是带密钥的摘要算法。
 *
 *     它适合用来做短注册码校验。
 */

#include "sha256.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace bhc {
namespace util {

namespace {

/*
 * SHA256_BLOCK_SIZE
 *
 * SHA256 的分组大小固定为 64 字节，也就是 512 bit。
 *
 * SHA256 算法会把输入数据切成若干个 64 字节分组，
 * 然后逐组进行压缩运算。
 */
static constexpr std::size_t SHA256_BLOCK_SIZE = 64;

/*
 * SHA256_DIGEST_SIZE
 *
 * SHA256 的输出摘要长度固定为 32 字节，也就是 256 bit。
 *
 * 最终 bytes_to_hex() 转换成十六进制字符串后，
 * 32 字节会变成 64 个十六进制字符。
 */
static constexpr std::size_t SHA256_DIGEST_SIZE = 32;

/*
 * kSha256Constants
 *
 * SHA256 标准中定义的 64 个常量。
 *
 * 每一轮压缩运算都会使用一个常量。
 *
 * 这些常量来自 SHA256 标准，不是项目自定义密钥，
 * 也不需要保密。
 */
static constexpr std::array<std::uint32_t, 64> kSha256Constants = {{
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
}};

/*
 * rotate_right()
 *
 * 功能：
 *
 *     对 32 位无符号整数做循环右移。
 *
 * 参数：
 *
 *     value：
 *
 *         要被循环右移的 32 位整数。
 *
 *     bits：
 *
 *         右移的位数。
 *
 * 为什么不是普通右移？
 *
 *     普通右移会把右边移出去的 bit 丢掉。
 *
 *     循环右移会把右边移出去的 bit 再放回左边。
 *
 * SHA256 标准中大量使用循环右移操作。
 */
std::uint32_t rotate_right(std::uint32_t value, std::uint32_t bits)
{
    return (value >> bits) | (value << (32u - bits));
}

/*
 * choose()
 *
 * SHA256 中的 Ch 函数。
 *
 * 逻辑含义：
 *
 *     对 x 的每一位进行判断：
 *
 *         如果 x 当前 bit 是 1，则选择 y 对应 bit；
 *         如果 x 当前 bit 是 0，则选择 z 对应 bit。
 *
 * 公式：
 *
 *     Ch(x, y, z) = (x & y) ^ (~x & z)
 */
std::uint32_t choose(std::uint32_t x, std::uint32_t y, std::uint32_t z)
{
    return (x & y) ^ (~x & z);
}

/*
 * majority()
 *
 * SHA256 中的 Maj 函数。
 *
 * 逻辑含义：
 *
 *     对 x、y、z 的每一位取多数值。
 *
 *     也就是说：
 *
 *         三个 bit 中至少两个为 1，则结果为 1；
 *         否则结果为 0。
 *
 * 公式：
 *
 *     Maj(x, y, z) = (x & y) ^ (x & z) ^ (y & z)
 */
std::uint32_t majority(std::uint32_t x, std::uint32_t y, std::uint32_t z)
{
    return (x & y) ^ (x & z) ^ (y & z);
}

/*
 * big_sigma0()
 *
 * SHA256 压缩函数中的大 Sigma0。
 *
 * 公式：
 *
 *     Σ0(x) = ROTR^2(x) ^ ROTR^13(x) ^ ROTR^22(x)
 */
std::uint32_t big_sigma0(std::uint32_t x)
{
    return rotate_right(x, 2u) ^ rotate_right(x, 13u) ^ rotate_right(x, 22u);
}

/*
 * big_sigma1()
 *
 * SHA256 压缩函数中的大 Sigma1。
 *
 * 公式：
 *
 *     Σ1(x) = ROTR^6(x) ^ ROTR^11(x) ^ ROTR^25(x)
 */
std::uint32_t big_sigma1(std::uint32_t x)
{
    return rotate_right(x, 6u) ^ rotate_right(x, 11u) ^ rotate_right(x, 25u);
}

/*
 * small_sigma0()
 *
 * SHA256 消息扩展中的小 sigma0。
 *
 * 公式：
 *
 *     σ0(x) = ROTR^7(x) ^ ROTR^18(x) ^ SHR^3(x)
 *
 * 其中 SHR 是普通右移。
 */
std::uint32_t small_sigma0(std::uint32_t x)
{
    return rotate_right(x, 7u) ^ rotate_right(x, 18u) ^ (x >> 3u);
}

/*
 * small_sigma1()
 *
 * SHA256 消息扩展中的小 sigma1。
 *
 * 公式：
 *
 *     σ1(x) = ROTR^17(x) ^ ROTR^19(x) ^ SHR^10(x)
 */
std::uint32_t small_sigma1(std::uint32_t x)
{
    return rotate_right(x, 17u) ^ rotate_right(x, 19u) ^ (x >> 10u);
}

/*
 * read_be32()
 *
 * 功能：
 *
 *     从 4 个字节中读取一个大端序 32 位整数。
 *
 * 参数：
 *
 *     data：
 *
 *         指向至少 4 个字节的指针。
 *
 * 大端序说明：
 *
 *     data[0] 是最高位字节；
 *     data[3] 是最低位字节。
 *
 * SHA256 标准使用大端序解释输入分组。
 */
std::uint32_t read_be32(const std::uint8_t* data)
{
    return (static_cast<std::uint32_t>(data[0]) << 24u) |
           (static_cast<std::uint32_t>(data[1]) << 16u) |
           (static_cast<std::uint32_t>(data[2]) << 8u)  |
           (static_cast<std::uint32_t>(data[3]));
}

/*
 * write_be32()
 *
 * 功能：
 *
 *     把一个 32 位整数按大端序写入 4 个字节。
 *
 * 参数：
 *
 *     value：
 *
 *         要写出的 32 位整数。
 *
 *     out：
 *
 *         输出缓冲区，必须至少有 4 字节空间。
 *
 * 用途：
 *
 *     SHA256 最终摘要由 8 个 32 位状态变量组成。
 *
 *     每个状态变量都需要按大端序写入最终 32 字节 digest。
 */
void write_be32(std::uint32_t value, std::uint8_t* out)
{
    out[0] = static_cast<std::uint8_t>((value >> 24u) & 0xffu);
    out[1] = static_cast<std::uint8_t>((value >> 16u) & 0xffu);
    out[2] = static_cast<std::uint8_t>((value >> 8u) & 0xffu);
    out[3] = static_cast<std::uint8_t>(value & 0xffu);
}

/*
 * write_be64()
 *
 * 功能：
 *
 *     把一个 64 位整数按大端序写入 8 个字节。
 *
 * 参数：
 *
 *     value：
 *
 *         要写出的 64 位整数。
 *
 *     out：
 *
 *         输出缓冲区，必须至少有 8 字节空间。
 *
 * 用途：
 *
 *     SHA256 填充阶段需要在消息最后写入原始消息长度。
 *
 *     该长度以 bit 为单位，并且占用 64 bit。
 */
void write_be64(std::uint64_t value, std::uint8_t* out)
{
    out[0] = static_cast<std::uint8_t>((value >> 56u) & 0xffu);
    out[1] = static_cast<std::uint8_t>((value >> 48u) & 0xffu);
    out[2] = static_cast<std::uint8_t>((value >> 40u) & 0xffu);
    out[3] = static_cast<std::uint8_t>((value >> 32u) & 0xffu);
    out[4] = static_cast<std::uint8_t>((value >> 24u) & 0xffu);
    out[5] = static_cast<std::uint8_t>((value >> 16u) & 0xffu);
    out[6] = static_cast<std::uint8_t>((value >> 8u) & 0xffu);
    out[7] = static_cast<std::uint8_t>(value & 0xffu);
}

/*
 * Sha256Context
 *
 * SHA256 计算过程中的上下文对象。
 *
 * 为什么需要上下文？
 *
 *     SHA256 可以分批处理数据。
 *
 *     例如一个大文件可以一段一段送入算法，而不用一次性全部放入内存。
 *
 * 当前授权模块一般输入字符串很短，但保留上下文结构可以让实现更标准。
 */
struct Sha256Context {
    /*
     * state
     *
     * SHA256 的 8 个内部状态变量。
     *
     * 每个状态变量是 32 bit。
     *
     * 初始化时使用 SHA256 标准规定的初始值。
     * 每处理一个 64 字节分组，state 都会被更新。
     */
    std::array<std::uint32_t, 8> state;

    /*
     * buffer
     *
     * 暂存不足 64 字节的数据。
     *
     * SHA256 每次压缩处理 64 字节。
     *
     * 如果 update() 输入的数据不是 64 的整数倍，
     * 剩余部分就暂存在 buffer 中，等后续数据凑够 64 字节再处理。
     */
    std::array<std::uint8_t, SHA256_BLOCK_SIZE> buffer;

    /*
     * buffer_size
     *
     * 当前 buffer 中已经存放了多少字节。
     *
     * 取值范围：
     *
     *     0 到 63
     */
    std::size_t buffer_size;

    /*
     * total_size
     *
     * 已经输入到 SHA256 的原始消息总字节数。
     *
     * 注意：
     *
     *     这里统计的是填充前的原始数据长度。
     *
     *     finalize() 阶段会把 total_size 转换成 bit 长度，
     *     写到最后一个分组末尾。
     */
    std::uint64_t total_size;
};

/*
 * sha256_init()
 *
 * 功能：
 *
 *     初始化 SHA256 上下文。
 *
 * 初始化内容：
 *
 *     1. 设置 8 个标准初始状态常量；
 *     2. 清空临时 buffer；
 *     3. 将 buffer_size 设为 0；
 *     4. 将 total_size 设为 0。
 */
void sha256_init(Sha256Context& ctx)
{
    ctx.state = {{
        0x6a09e667u,
        0xbb67ae85u,
        0x3c6ef372u,
        0xa54ff53au,
        0x510e527fu,
        0x9b05688cu,
        0x1f83d9abu,
        0x5be0cd19u
    }};

    ctx.buffer.fill(0);
    ctx.buffer_size = 0;
    ctx.total_size = 0;
}

/*
 * sha256_compress()
 *
 * 功能：
 *
 *     SHA256 核心压缩函数。
 *
 * 参数：
 *
 *     ctx：
 *
 *         SHA256 上下文，里面保存当前 8 个状态变量。
 *
 *     block：
 *
 *         一个完整的 64 字节消息分组。
 *
 * 处理流程：
 *
 *     1. 把 64 字节分组成 16 个 32 位大端整数；
 *     2. 扩展成 64 个 32 位消息字；
 *     3. 使用 64 轮运算更新临时变量；
 *     4. 把结果加回 ctx.state。
 */
void sha256_compress(Sha256Context& ctx, const std::uint8_t* block)
{
    /*
     * w 是消息调度数组。
     *
     * 前 16 个元素来自当前 64 字节分组。
     * 后 48 个元素由前面的元素扩展得到。
     */
    std::array<std::uint32_t, 64> w;

    /*
     * 读取前 16 个 32 位字。
     *
     * 每 4 个字节组成一个 uint32_t。
     */
    for (std::size_t i = 0; i < 16; ++i) {
        w[i] = read_be32(block + i * 4);
    }

    /*
     * 生成后 48 个消息字。
     *
     * 公式：
     *
     *     w[i] = σ1(w[i-2]) + w[i-7] + σ0(w[i-15]) + w[i-16]
     *
     * uint32_t 溢出会自动按 2^32 取模，
     * 这正是 SHA256 标准需要的行为。
     */
    for (std::size_t i = 16; i < 64; ++i) {
        w[i] = small_sigma1(w[i - 2]) +
               w[i - 7] +
               small_sigma0(w[i - 15]) +
               w[i - 16];
    }

    /*
     * a 到 h 是本轮压缩的临时工作变量。
     *
     * 初始值来自 ctx.state。
     */
    std::uint32_t a = ctx.state[0];
    std::uint32_t b = ctx.state[1];
    std::uint32_t c = ctx.state[2];
    std::uint32_t d = ctx.state[3];
    std::uint32_t e = ctx.state[4];
    std::uint32_t f = ctx.state[5];
    std::uint32_t g = ctx.state[6];
    std::uint32_t h = ctx.state[7];

    /*
     * 64 轮主循环。
     *
     * 每一轮使用：
     *
     *     1. 当前状态变量；
     *     2. 一个消息字 w[i]；
     *     3. 一个标准常量 kSha256Constants[i]。
     */
    for (std::size_t i = 0; i < 64; ++i) {
        /*
         * temp1 对应 SHA256 标准中的 T1。
         */
        std::uint32_t temp1 = h +
                              big_sigma1(e) +
                              choose(e, f, g) +
                              kSha256Constants[i] +
                              w[i];

        /*
         * temp2 对应 SHA256 标准中的 T2。
         */
        std::uint32_t temp2 = big_sigma0(a) +
                              majority(a, b, c);

        /*
         * 更新 8 个工作变量。
         *
         * 注意更新顺序不能随意改变。
         */
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    /*
     * 把本分组压缩结果累加回上下文状态。
     *
     * SHA256 的压缩是迭代式的：
     *
     *     新 state = 旧 state + 当前分组压缩结果
     */
    ctx.state[0] += a;
    ctx.state[1] += b;
    ctx.state[2] += c;
    ctx.state[3] += d;
    ctx.state[4] += e;
    ctx.state[5] += f;
    ctx.state[6] += g;
    ctx.state[7] += h;
}

/*
 * sha256_update()
 *
 * 功能：
 *
 *     向 SHA256 上下文输入一段数据。
 *
 * 参数：
 *
 *     ctx：
 *
 *         SHA256 上下文。
 *
 *     data：
 *
 *         输入数据指针。
 *
 *     length：
 *
 *         输入数据字节数。
 *
 * 处理逻辑：
 *
 *     1. 如果 data 为空或 length 为 0，直接返回；
 *     2. 如果 ctx.buffer 中已有残留数据，优先补满 64 字节；
 *     3. 对完整的 64 字节分组逐个压缩；
 *     4. 剩余不足 64 字节的数据保存到 ctx.buffer。
 */
void sha256_update(Sha256Context& ctx, const std::uint8_t* data, std::size_t length)
{
    /*
     * 空输入不需要处理。
     *
     * 这里判断 data == nullptr 是为了防止调用者传入空指针。
     *
     * length == 0 表示没有字节需要处理。
     */
    if (data == nullptr || length == 0) {
        return;
    }

    /*
     * total_size 记录的是原始输入总字节数。
     *
     * finalize() 阶段会用它计算消息 bit 长度。
     */
    ctx.total_size += static_cast<std::uint64_t>(length);

    /*
     * offset 表示当前已经从 data 中消费了多少字节。
     */
    std::size_t offset = 0;

    /*
     * 如果 buffer 中已经有残留数据，需要先尝试补满一个 64 字节分组。
     */
    if (ctx.buffer_size > 0) {
        /*
         * need 表示还需要多少字节才能把 buffer 补满 64 字节。
         */
        const std::size_t need = SHA256_BLOCK_SIZE - ctx.buffer_size;

        /*
         * copy_size 表示本次实际能复制多少字节。
         *
         * 如果 length < need，说明还不够补满；
         * 如果 length >= need，说明可以补满并进行压缩。
         */
        const std::size_t copy_size = (length < need) ? length : need;

        std::memcpy(ctx.buffer.data() + ctx.buffer_size, data, copy_size);

        ctx.buffer_size += copy_size;
        offset += copy_size;

        /*
         * 如果 buffer 已经满 64 字节，就压缩这个分组。
         */
        if (ctx.buffer_size == SHA256_BLOCK_SIZE) {
            sha256_compress(ctx, ctx.buffer.data());
            ctx.buffer_size = 0;
        }
    }

    /*
     * 处理 data 中剩余的完整 64 字节分组。
     */
    while (offset + SHA256_BLOCK_SIZE <= length) {
        sha256_compress(ctx, data + offset);
        offset += SHA256_BLOCK_SIZE;
    }

    /*
     * 如果最后还有不足 64 字节的数据，保存到 buffer 中。
     */
    if (offset < length) {
        ctx.buffer_size = length - offset;
        std::memcpy(ctx.buffer.data(), data + offset, ctx.buffer_size);
    }
}

/*
 * sha256_finalize()
 *
 * 功能：
 *
 *     完成 SHA256 计算并输出最终 32 字节摘要。
 *
 * SHA256 填充规则：
 *
 *     1. 在消息后添加一个 bit 1，也就是字节 0x80；
 *     2. 后面添加若干个 bit 0；
 *     3. 直到最后 8 字节可以存放原始消息长度；
 *     4. 最后 8 字节写入原始消息 bit 长度，大端序。
 *
 * 参数：
 *
 *     ctx：
 *
 *         SHA256 上下文。
 *
 * 返回值：
 *
 *     32 字节 SHA256 摘要。
 */
Bytes sha256_finalize(Sha256Context& ctx)
{
    /*
     * digest 用于保存最终输出。
     *
     * 长度固定为 32 字节。
     */
    Bytes digest(SHA256_DIGEST_SIZE);

    /*
     * bit_length 是原始消息长度，单位是 bit。
     *
     * total_size 是字节数，所以需要乘以 8。
     */
    const std::uint64_t bit_length = ctx.total_size * 8u;

    /*
     * 先追加 0x80。
     *
     * 二进制含义是：
     *
     *     10000000
     *
     * 它代表 SHA256 填充规则中的“追加一个 bit 1”，
     * 后面跟着若干 bit 0。
     */
    ctx.buffer[ctx.buffer_size++] = 0x80u;

    /*
     * 如果当前 buffer 已经放不下最后 8 字节长度字段，
     * 那么需要先把当前分组补 0 并压缩。
     *
     * 为什么是 56？
     *
     *     SHA256 分组大小是 64 字节。
     *
     *     最后 8 字节要存放原始消息长度。
     *
     *     所以前面数据和 padding 最多只能占到第 56 字节。
     */
    if (ctx.buffer_size > 56) {
        while (ctx.buffer_size < SHA256_BLOCK_SIZE) {
            ctx.buffer[ctx.buffer_size++] = 0;
        }

        sha256_compress(ctx, ctx.buffer.data());
        ctx.buffer_size = 0;
    }

    /*
     * 当前 buffer_size <= 56。
     *
     * 将 56 之前的剩余空间全部补 0。
     */
    while (ctx.buffer_size < 56) {
        ctx.buffer[ctx.buffer_size++] = 0;
    }

    /*
     * 在最后 8 字节写入原始消息 bit 长度。
     */
    write_be64(bit_length, ctx.buffer.data() + 56);

    /*
     * 压缩最后一个分组。
     */
    sha256_compress(ctx, ctx.buffer.data());

    /*
     * 把 8 个 32 位 state 写入 digest。
     *
     * 每个 state 写 4 字节，总共 8 * 4 = 32 字节。
     */
    for (std::size_t i = 0; i < ctx.state.size(); ++i) {
        write_be32(ctx.state[i], digest.data() + i * 4);
    }

    return digest;
}

/*
 * string_to_bytes()
 *
 * 功能：
 *
 *     将 std::string 转换为 Bytes。
 *
 * 注意：
 *
 *     这里不会做字符编码转换。
 *
 *     字符串中的每个 char 都直接当作一个字节。
 */
Bytes string_to_bytes(const std::string& text)
{
    return Bytes(text.begin(), text.end());
}

} // anonymous namespace


Bytes sha256(const Bytes& data)
{
    /*
     * 创建 SHA256 上下文。
     *
     * ctx 保存中间状态、临时分组和输入长度。
     */
    Sha256Context ctx;

    /*
     * 初始化上下文。
     */
    sha256_init(ctx);

    /*
     * 如果 data 非空，则把 data 内容送入 SHA256。
     *
     * data.data() 返回底层字节指针。
     * data.size() 返回字节数量。
     */
    if (!data.empty()) {
        sha256_update(ctx, data.data(), data.size());
    }

    /*
     * 完成填充并输出最终摘要。
     */
    return sha256_finalize(ctx);
}


Bytes sha256(const std::string& text)
{
    /*
     * 字符串版本的 sha256。
     *
     * 直接把 text 的底层字节作为 SHA256 输入。
     */
    Sha256Context ctx;
    sha256_init(ctx);

    if (!text.empty()) {
        sha256_update(
            ctx,
            reinterpret_cast<const std::uint8_t*>(text.data()),
            text.size()
        );
    }

    return sha256_finalize(ctx);
}


std::string sha256_hex(const std::string& text)
{
    /*
     * 先计算二进制 SHA256 摘要，
     * 再转换为十六进制字符串。
     */
    return bytes_to_hex(sha256(text));
}


Bytes hmac_sha256(const std::string& key, const std::string& message)
{
    /*
     * HMAC-SHA256 公式：
     *
     *     HMAC(key, message) =
     *         SHA256((key XOR opad) || SHA256((key XOR ipad) || message))
     *
     * 其中：
     *
     *     ipad = 0x36 重复 64 次
     *     opad = 0x5c 重复 64 次
     *
     * SHA256 的 block size 是 64 字节。
     */

    /*
     * 把 key 转为字节数组。
     */
    Bytes key_bytes = string_to_bytes(key);

    /*
     * 如果 key 长度超过 SHA256 分组大小 64 字节，
     * 按 HMAC 标准，需要先对 key 做 SHA256，
     * 得到 32 字节摘要作为新的 key。
     */
    if (key_bytes.size() > SHA256_BLOCK_SIZE) {
        key_bytes = sha256(key_bytes);
    }

    /*
     * 将 key 扩展或补零到 64 字节。
     *
     * HMAC 要求内部使用 block size 长度的 key。
     *
     * 如果原始 key 不足 64 字节，后面补 0。
     */
    key_bytes.resize(SHA256_BLOCK_SIZE, 0x00u);

    /*
     * inner_pad 对应 key XOR ipad。
     *
     * outer_pad 对应 key XOR opad。
     */
    Bytes inner_pad(SHA256_BLOCK_SIZE);
    Bytes outer_pad(SHA256_BLOCK_SIZE);

    /*
     * 对 key 的每个字节分别和 ipad / opad 做异或。
     */
    for (std::size_t i = 0; i < SHA256_BLOCK_SIZE; ++i) {
        inner_pad[i] = static_cast<std::uint8_t>(key_bytes[i] ^ 0x36u);
        outer_pad[i] = static_cast<std::uint8_t>(key_bytes[i] ^ 0x5cu);
    }

    /*
     * inner_data = inner_pad || message
     *
     * 也就是把 inner_pad 和 message 拼接起来。
     */
    Bytes inner_data;
    inner_data.reserve(inner_pad.size() + message.size());

    inner_data.insert(inner_data.end(), inner_pad.begin(), inner_pad.end());
    inner_data.insert(inner_data.end(), message.begin(), message.end());

    /*
     * inner_hash = SHA256(inner_data)
     */
    Bytes inner_hash = sha256(inner_data);

    /*
     * outer_data = outer_pad || inner_hash
     */
    Bytes outer_data;
    outer_data.reserve(outer_pad.size() + inner_hash.size());

    outer_data.insert(outer_data.end(), outer_pad.begin(), outer_pad.end());
    outer_data.insert(outer_data.end(), inner_hash.begin(), inner_hash.end());

    /*
     * HMAC 最终结果：
     *
     *     SHA256(outer_data)
     */
    return sha256(outer_data);
}


std::string hmac_sha256_hex(const std::string& key, const std::string& message)
{
    /*
     * 先计算 HMAC-SHA256 二进制结果，
     * 再转换为十六进制字符串。
     */
    return bytes_to_hex(hmac_sha256(key, message));
}


std::string bytes_to_hex(const Bytes& bytes)
{
    /*
     * std::ostringstream 用于构造输出字符串。
     */
    std::ostringstream oss;

    /*
     * std::hex 表示后续整数以十六进制输出。
     *
     * std::setfill('0') 表示如果宽度不足，用字符 '0' 补齐。
     */
    oss << std::hex << std::setfill('0');

    /*
     * 逐字节转换。
     *
     * 每个字节输出为两个十六进制字符。
     */
    for (std::uint8_t byte : bytes) {
        /*
         * std::setw(2) 表示当前字段宽度至少为 2。
         *
         * static_cast<int>(byte) 是为了让 uint8_t 按数字输出，
         * 而不是被当作字符输出。
         */
        oss << std::setw(2) << static_cast<int>(byte);
    }

    return oss.str();
}


/*
 * hex_value()
 *
 * 功能：
 *
 *     将单个十六进制字符转换为数值。
 *
 * 返回值：
 *
 *     0 到 15：
 *
 *         表示合法十六进制字符。
 *
 *     -1：
 *
 *         表示非法字符。
 */
int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }

    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }

    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }

    return -1;
}


Bytes hex_to_bytes(const std::string& hex)
{
    /*
     * 十六进制字符串必须是偶数长度。
     *
     * 因为每两个十六进制字符对应一个字节。
     *
     * 例如：
     *
     *     "AB" -> 0xAB
     *
     * 如果长度是奇数，说明输入不完整，直接返回空数组。
     */
    if (hex.size() % 2 != 0) {
        return Bytes();
    }

    /*
     * 提前分配输出空间。
     *
     * hex 长度为 2N，则输出字节数为 N。
     */
    Bytes bytes;
    bytes.reserve(hex.size() / 2);

    /*
     * 每次读取两个十六进制字符。
     */
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        /*
         * high 是高 4 bit。
         * low 是低 4 bit。
         */
        const int high = hex_value(hex[i]);
        const int low  = hex_value(hex[i + 1]);

        /*
         * 如果任一字符非法，则返回空数组。
         *
         * 这样可以避免抛异常，授权模块使用时更稳定。
         */
        if (high < 0 || low < 0) {
            return Bytes();
        }

        /*
         * 将两个 4 bit 合并成一个字节。
         *
         * 例如：
         *
         *     high = 0xA
         *     low  = 0xB
         *
         *     byte = 0xAB
         */
        const std::uint8_t byte =
            static_cast<std::uint8_t>((high << 4) | low);

        bytes.push_back(byte);
    }

    return bytes;
}

} // namespace util
} // namespace bhc