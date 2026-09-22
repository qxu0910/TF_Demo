#include "byte_sum.hpp"
#include <cuda_runtime.h>
#include <stdexcept>
#include <string>
#include <chrono>

namespace factory::compute {
namespace {
void check(cudaError_t status, const char* operation) {
    if (status != cudaSuccess)
        throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(status));
}
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
class Stream {
public:
    Stream() { check(cudaStreamCreateWithFlags(&value, cudaStreamNonBlocking), "create stream"); }
    // 异常路径同样先等待本次工作，再由外层对象释放显存。
    ~Stream() { cudaStreamSynchronize(value); cudaStreamDestroy(value); }
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;
    cudaStream_t value{};
};
class Event {
public:
    Event() { check(cudaEventCreate(&value), "create event"); }
    ~Event() { cudaEventDestroy(value); }
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    cudaEvent_t value{};
};

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

Sample measure_byte_sum(std::string_view input) {
    if (input.size() > 8192) throw std::invalid_argument("byte_sum limit is 8192 bytes");
    const auto total_start = Clock::now();
    Sample result;
    if (input.empty()) { result.total_ms = elapsed(total_start); return result; }
    check(cudaSetDevice(0), "cudaSetDevice");
    {
    DeviceBuffer<unsigned char> device_input(input.size());
    DeviceBuffer<unsigned int> device_output(1);
    Stream stream;
    Event start, stop;
    auto stage = Clock::now();
    check(cudaMemcpyAsync(device_input.get(), input.data(), input.size(), cudaMemcpyHostToDevice, stream.value), "copy input");
    check(cudaStreamSynchronize(stream.value), "wait input");
    result.h2d_ms = elapsed(stage);
    check(cudaMemsetAsync(device_output.get(), 0, sizeof(unsigned int), stream.value), "clear output");
    check(cudaEventRecord(start.value, stream.value), "record start");
    const int blocks = static_cast<int>((input.size() + 255) / 256);
    sum_kernel<<<blocks, 256, 0, stream.value>>>(device_input.get(), device_output.get(), static_cast<int>(input.size()));
    check(cudaGetLastError(), "launch sum_kernel");
    check(cudaEventRecord(stop.value, stream.value), "record stop");
    check(cudaEventSynchronize(stop.value), "wait kernel");
    float kernel_ms = 0;
    check(cudaEventElapsedTime(&kernel_ms, start.value, stop.value), "kernel elapsed");
    result.kernel_ms = kernel_ms;
    stage = Clock::now();
    check(cudaMemcpyAsync(&result.sum, device_output.get(), sizeof(unsigned int), cudaMemcpyDeviceToHost, stream.value), "copy result");
    check(cudaStreamSynchronize(stream.value), "wait result");
    result.d2h_ms = elapsed(stage);
    } // total 包含资源创建与销毁；阶段之和不等于 total。
    result.total_ms = elapsed(total_start);
    return result;
}
std::uint32_t byte_sum(std::string_view input) { return measure_byte_sum(input).sum; }
const char* backend() { return "cpp-cuda-demo"; }
void check_backend() {
    if (byte_sum("ABC") != 198) throw std::runtime_error("CUDA startup self-check failed");
}
}
