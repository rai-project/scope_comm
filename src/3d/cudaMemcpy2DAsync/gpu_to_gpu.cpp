#include <sstream>

#include "scope/scope.hpp"

#include "../args.hpp"

#define NAME "Comm_3d_cudaMemcpy2DAsync_GPUToGPU"

auto Comm_3d_cudaMemcpy2DAsync_GPUToGPU = [](benchmark::State &state, const int gpu0, const int gpu1) {

#if SCOPE_USE_NVTX == 1
  {
    std::stringstream name;
    name << NAME << "/" << gpu0 << "/" << gpu1 << "/" << state.range(0) << "/" << state.range(1) << "/"
         << state.range(2);
    nvtxRangePush(name.str().c_str());
  }
#endif

  OR_SKIP_AND_RETURN(cuda_reset_device(gpu0), NAME " failed to reset CUDA device");
  OR_SKIP_AND_RETURN(cuda_reset_device(gpu1), NAME " failed to reset CUDA device");

  // Create One stream per copy
  cudaStream_t stream = nullptr;
  OR_SKIP_AND_RETURN(cudaStreamCreate(&stream), NAME "failed to create stream");

  // Start and stop events for each copy
  cudaEvent_t start = nullptr;
  cudaEvent_t stop  = nullptr;
  OR_SKIP_AND_RETURN(cudaEventCreate(&start), NAME " failed to create event");
  OR_SKIP_AND_RETURN(cudaEventCreate(&stop), NAME " failed to create event");

  // target size to transfer
  cudaExtent copyExt;
  copyExt.width  = static_cast<size_t>(state.range(0));
  copyExt.height = static_cast<size_t>(state.range(1));
  copyExt.depth  = static_cast<size_t>(state.range(2));
  const size_t copyBytes = copyExt.width * copyExt.height * copyExt.depth;

  // properties of the allocation
  cudaExtent allocExt;
  allocExt.width  = 512; // how many bytes in a row
  allocExt.height = 512; // how many rows in a plane
  allocExt.depth  = 512;

  cudaPitchedPtr src, dst;

  // allocate on gpu0 and enable peer access
  OR_SKIP_AND_RETURN(cudaSetDevice(gpu0), NAME "failed to set device");
  OR_SKIP_AND_RETURN(cudaMalloc3D(&src, allocExt), NAME " failed to perform cudaMalloc3D");
  allocExt.width = src.pitch;
  OR_SKIP_AND_RETURN(cudaMemset3D(src, 0, allocExt), NAME " failed to perform src cudaMemset");
  if (gpu0 != gpu1) {
    cudaError_t err = cudaDeviceEnablePeerAccess(gpu1, 0);
    if (cudaSuccess != err && cudaErrorPeerAccessAlreadyEnabled != err) {
      state.SkipWithError(NAME " failed to ensure peer access");
    }
  }

  // allocate on gpu1 and enable peer access
  OR_SKIP_AND_RETURN(cudaSetDevice(gpu1), NAME "failed to set device");
  OR_SKIP_AND_RETURN(cudaMalloc3D(&dst, allocExt), NAME " failed to perform cudaMalloc3D");
  OR_SKIP_AND_RETURN(cudaMemset3D(dst, 0, allocExt), NAME " failed to perform dst cudaMemset");
  if (gpu0 != gpu1) {
    cudaError_t err = cudaDeviceEnablePeerAccess(gpu0, 0);
    if (cudaSuccess != err && cudaErrorPeerAccessAlreadyEnabled != err) {
      state.SkipWithError(NAME " failed to ensure peer access");
    }
  }

  for (auto _ : state) {
    // Start copy
    OR_SKIP_AND_BREAK(cudaEventRecord(start, stream), NAME " failed to record start event");

    // use a bunch of 2D copies to do the 3D copy
    for (size_t z = 0; z < copyExt.depth; ++z) {
      void *srcP = (char*)src.ptr + allocExt.width * allocExt.height * z;
      void *dstP = (char*)dst.ptr + allocExt.width * allocExt.height * z;

      OR_SKIP_AND_BREAK(cudaMemcpy2DAsync(
          dstP, allocExt.width, srcP, allocExt.width, copyExt.width, copyExt.height, cudaMemcpyDefault, stream), NAME " failed to start 2D copy");
    }

    OR_SKIP_AND_BREAK(cudaEventRecord(stop, stream), NAME " failed to record stop event");

    // Wait for all copies to finish
    OR_SKIP_AND_BREAK(cudaEventSynchronize(stop), NAME " failed to synchronize");

    // Get the transfer time
    float millis;
    OR_SKIP_AND_BREAK(cudaEventElapsedTime(&millis, start, stop), NAME " failed to compute elapsed tiume");
    state.SetIterationTime(millis / 1000);
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(copyBytes));
  state.counters["bytes"] = copyBytes;
  state.counters["gpu0"]  = gpu0;
  state.counters["gpu1"]  = gpu1;

  OR_SKIP_AND_RETURN(cudaEventDestroy(start), "cudaEventDestroy");
  OR_SKIP_AND_RETURN(cudaEventDestroy(stop), "cudaEventDestroy");
  OR_SKIP_AND_RETURN(cudaStreamDestroy(stream), "cudaStreamDestroy");
  OR_SKIP_AND_RETURN(cudaFree(src.ptr), "cudaFree");
  OR_SKIP_AND_RETURN(cudaFree(dst.ptr), "cudaFree");

#if SCOPE_USE_NVTX == 1
  nvtxRangePop();
#endif
};

static void registerer() {
  std::string name;
  for (size_t i = 0; i < unique_cuda_device_ids().size(); ++i) {
    for (size_t j = i; j < unique_cuda_device_ids().size(); ++j) {
      auto gpu0 = unique_cuda_device_ids()[i];
      auto gpu1 = unique_cuda_device_ids()[j];
      int ok1, ok2;
      if (!PRINT_IF_ERROR(cudaDeviceCanAccessPeer(&ok1, gpu0, gpu1)) &&
          !PRINT_IF_ERROR(cudaDeviceCanAccessPeer(&ok2, gpu1, gpu0))) {
        if ((ok1 && ok2) || i == j) {
          name = std::string(NAME) + "/" + std::to_string(gpu0) + "/" + std::to_string(gpu1);
          benchmark::RegisterBenchmark(name.c_str(), Comm_3d_cudaMemcpy2DAsync_GPUToGPU, gpu0, gpu1)
              ->TINY_ARGS()
              ->UseManualTime();
        }
      }
    }
  }
}

SCOPE_AFTER_INIT(registerer, NAME);
