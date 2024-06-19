#include <chrono>
#include <iostream>
#include <random>
#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include "Foundation/Foundation.hpp"
#include "Metal/Metal.hpp"
using namespace std;
using namespace chrono;

class Clock {
  Clock() { start = steady_clock::now(); }

  ~Clock() {
    end = steady_clock::now();
    auto duration = duration_cast<microseconds>(end - start);
    std::cout << "it cost " << duration.count() / 1000.f << " ms" << std::endl;
  }

 public:
  steady_clock::time_point start;
  steady_clock::time_point end;
};

void GenerateRandom(float* buffer, int64_t data_len) {
  for (int64_t i = 0; i < data_len; i++) {
    std::mt19937 engine{
        std::random_device{}()};  // std::random_device{}() 用于获取随机种子

    // // 定义随机数分布，这里是0到100（含）的均匀分布
    std::uniform_int_distribution<int> dist(0, 100);

    // // 生成随机数
    int random_number = dist(engine);
    buffer[i] = random_number;
  }
}

void GenerateRandomFloatData(MTL::Buffer* buffer, int64_t data_len) {
  // The pointer needs to be explicitly cast in C++, a difference from
  // Objective-C.
  float* dataPtr = (float*)buffer->contents();

  for (unsigned long index = 0; index < data_len; index++) {
    dataPtr[index] = (float)rand() / (float)(RAND_MAX);
    // printf("%f\n", dataPtr[index]);
  }
}

int main() {
  // prepare input output

  int64_t data_len = pow(2, 20);

  float* input1 = new float[data_len];
  float* input2 = new float[data_len];
  float* result = new float[data_len];

  GenerateRandom(input1, data_len);
  GenerateRandom(input2, data_len);

  NS::Error* err = nullptr;

  // 1. get device handler
  MTL::Device* device = MTL::CreateSystemDefaultDevice();
  // 2. create func
  auto file_path =
      NS::String::string("./add.metallib", NS::ASCIIStringEncoding);
  MTL::Library* default_libray = device->newLibrary(file_path, &err);
  if (default_libray == nullptr) {
    std::cout << "Failed to find the default library. Error: "
              << err->description()->utf8String() << std::endl;
    ;

    return 0;
  }
  auto func_name = NS::String::string("add_arrays", NS::ASCIIStringEncoding);
  MTL::Function* add_fun = default_libray->newFunction(func_name);
  default_libray->release();
  if (add_fun == nullptr) {
    std::cout << "failed to get fun" << std::endl;

    return 0;
  }

  // 3. create piplinestate
  MTL::ComputePipelineState* piplinestate =
      device->newComputePipelineState(add_fun, &err);

  add_fun->release();
  if (piplinestate == nullptr) {
    std::cout << "create pipline failed" << std::endl;
    return 0;
  }

  // 4. create command queue
  MTL::CommandQueue* cmd_queue = device->newCommandQueue();
  if (cmd_queue == nullptr) {
    std::cout << "create command queue failed" << std::endl;
    return 0;
  }

  // 5. create buffer

  MTL::Buffer* buffera = device->newBuffer(data_len * sizeof(float),
                                           MTL::ResourceStorageModeShared);
  MTL::Buffer* bufferb = device->newBuffer(data_len * sizeof(float),
                                           MTL::ResourceStorageModeShared);

  MTL::Buffer* bufferc = device->newBuffer(data_len * sizeof(float),
                                           MTL::ResourceStorageModeShared);

  memcpy((buffera->contents()), (void*)input1, data_len * sizeof(float));
  memcpy((bufferb->contents()), (void*)input2, data_len * sizeof(float));
  //   GenerateRandomFloatData(buffera, data_len);
  //   GenerateRandomFloatData(bufferb, data_len);

  // 6. create command buffer
  MTL::CommandBuffer* cmd_buffer = cmd_queue->commandBuffer();
  assert(cmd_buffer != nullptr);
  MTL::ComputeCommandEncoder* cmd_encoder = cmd_buffer->computeCommandEncoder();
  assert(cmd_encoder != nullptr);

  // 7. set the command encoder;
  cmd_encoder->setComputePipelineState(piplinestate);
  cmd_encoder->setBuffer(buffera, 0, 0);
  cmd_encoder->setBuffer(bufferb, 0, 1);
  cmd_encoder->setBuffer(bufferc, 0, 2);

  MTL::Size gridSize = MTL::Size::Make(data_len, 1, 1);
  NS::UInteger threadGroupSize = piplinestate->maxTotalThreadsPerThreadgroup();
  if (threadGroupSize > data_len) {
    threadGroupSize = data_len;
  }
  MTL::Size threadgroupSize = MTL::Size::Make(threadGroupSize, 1, 1);
  cmd_encoder->dispatchThreads(gridSize, threadgroupSize);

  cmd_encoder->endEncoding();
  cmd_buffer->commit();
  cmd_buffer->waitUntilCompleted();

  float* gpu_result = (float*)(bufferc->contents());
  for (int64_t i = 0; i < data_len; i++) {
    if (gpu_result[i] != input1[i] + input2[i]) {
      printf("Compute ERROR: index=%lu result=%g vs %g=a+b\n", index,
             gpu_result[i], input1[i] + input2[i]);
      assert(gpu_result[i] == (input2[i] + input1[i]));
    }
  }

  delete[] input1;
  delete[] input2;
  delete[] result;
  return 0;
}
