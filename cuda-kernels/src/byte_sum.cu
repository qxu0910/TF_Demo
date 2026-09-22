#include "byte_sum.hpp"
#include <cuda_runtime.h>
#include <stdexcept>
#include <string>

namespace factory::compute {
namespace {
void check(cudaError_t status, const char* operation) {
    if (status != cudaSuccess)
        throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(status));
}

// 每次调用独占显存，多个 C++ worker 不共享输出地址。
template<class T> class DeviceBuffer {
public:
    explicit DeviceBuffer(std::size_t count) {
        check(cudaMalloc(reinterpret_cast<void**>(&pointer_), count * sizeof(T)), "cudaMalloc");
    }
    ~DeviceBuffer() { if (pointer_) cudaFree(pointer_); }
    DeviceBuffer(const DeviceBuffer&) = delete;
    DeviceBuffer& operator=(const DeviceBuffer&) = delete;
    T* get() const { return pointer_; }
private:
    T* pointer_ = nullptr;
};

// 一个 block 处理 256 个字节，先在共享内存做树形归约，再原子累加。
// 尾部不足 256 的线程填零，但仍必须参加每一次 __syncthreads()。
__global__ void sum_kernel(const unsigned char* input, unsigned int* output, int size) {
    __shared__ unsigned int partial[256];
    const unsigned int lane = threadIdx.x;
    const unsigned int index = blockIdx.x * blockDim.x + lane;
    partial[lane] = index < static_cast<unsigned int>(size) ? input[index] : 0;
    __syncthreads();
    for (unsigned int stride = blockDim.x / 2; stride > 0; stride /= 2) {
        if (lane < stride) partial[lane] += partial[lane + stride];
        __syncthreads();
    }
    if (lane == 0) atomicAdd(output, partial[0]);
}
}

std::uint32_t byte_sum(std::string_view input) {
    if (input.size() > 8192) throw std::invalid_argument("byte_sum limit is 8192 bytes");
    if (input.empty()) return 0; // 不发起零 block 的 kernel。
    check(cudaSetDevice(0), "cudaSetDevice");
    DeviceBuffer<unsigned char> device_input(input.size());
    DeviceBuffer<unsigned int> device_output(1);
    check(cudaMemcpy(device_input.get(), input.data(), input.size(), cudaMemcpyHostToDevice), "copy input");
    check(cudaMemset(device_output.get(), 0, sizeof(unsigned int)), "clear output");
    const int blocks = static_cast<int>((input.size() + 255) / 256);
    sum_kernel<<<blocks, 256>>>(device_input.get(), device_output.get(), static_cast<int>(input.size()));
    check(cudaGetLastError(), "launch sum_kernel");
    // 同步拷回结果：检查执行错误，返回前确保输入显存不再被 kernel 使用。
    unsigned int result = 0;
    check(cudaMemcpy(&result, device_output.get(), sizeof(result), cudaMemcpyDeviceToHost), "copy result");
    return result; // 最大 8192 * 255 = 2088960，不会溢出 32 位。
}
const char* backend() { return "cpp-cuda-demo"; }
void check_backend() {
    if (byte_sum("ABC") != 198) throw std::runtime_error("CUDA startup self-check failed");
}
}
