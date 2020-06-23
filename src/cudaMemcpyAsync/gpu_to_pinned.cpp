 #include "scope/scope.hpp"

#include "args.hpp"

#define NAME "Comm_cudaMemcpyAsync_GPUToPinned"

auto Comm_cudaMemcpyAsync_GPUToPinned = [](benchmark::State &state, const int numa_id, const int cuda_id, const bool flush) {

  const auto bytes  = 1ULL << static_cast<size_t>(state.range(0));

  numa::bind_node(numa_id);
  if (PRINT_IF_ERROR(cuda_reset_device(cuda_id))) {
    state.SkipWithError(NAME " failed to reset CUDA device");
    return;
  }

  char *src = nullptr;
  void *dst = aligned_alloc(page_size(), bytes);
  if (PRINT_IF_ERROR(cudaHostRegister(dst, bytes, cudaHostRegisterPortable))) {
    state.SkipWithError(NAME " failed to register allocations");
    return;
  }
  defer(cudaHostUnregister(dst));
  defer(free(dst));
  std::memset(dst, 0, bytes);

  if (PRINT_IF_ERROR(cudaSetDevice(cuda_id))) {
    state.SkipWithError(NAME " failed to set CUDA device");
    return;
  }

  if (PRINT_IF_ERROR(cudaMalloc(&src, bytes))) {
    state.SkipWithError(NAME " failed to perform cudaMalloc");
    return;
  }
  defer(cudaFree(src));
  if (PRINT_IF_ERROR(cudaMemset(src, 0, bytes))) {
    state.SkipWithError(NAME " failed to perform cudaMemset");
    return;
  }

  cudaEvent_t start, stop;
  PRINT_IF_ERROR(cudaEventCreate(&start));
  PRINT_IF_ERROR(cudaEventCreate(&stop));

  for (auto _ : state) {
    std::memset(dst, 0, bytes);
    if (flush) {
      flush_all(dst, bytes);
    }
    cudaEventRecord(start, NULL);
    const auto cuda_err = cudaMemcpyAsync(dst, src, bytes, cudaMemcpyDeviceToHost);
    cudaEventRecord(stop, NULL);
    cudaEventSynchronize(stop);

    if (PRINT_IF_ERROR(cuda_err) != cudaSuccess) {
      state.SkipWithError(NAME " failed to perform memcpy");
      break;
    }

    float msecTotal = 0.0f;
    if (PRINT_IF_ERROR(cudaEventElapsedTime(&msecTotal, start, stop))) {
      state.SkipWithError(NAME " failed to get elapsed time");
      break;
    }
    state.SetIterationTime(msecTotal / 1000);
  }
  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(bytes));
  state.counters["bytes"] = bytes;
  state.counters["cuda_id"] = cuda_id;
  state.counters["numa_id"] = numa_id;

  // reset to run on any node
  numa::bind_node(-1);
};

static void registerer() {
  std::string name;
  for (auto cuda_id : unique_cuda_device_ids()) {
    for (auto numa_id : numa::ids()) {
      name = std::string(NAME) + "/" + std::to_string(numa_id) + "/" + std::to_string(cuda_id);
      benchmark::RegisterBenchmark(name.c_str(), Comm_cudaMemcpyAsync_GPUToPinned, numa_id, cuda_id, false)->SMALL_ARGS()->UseManualTime();
      name = std::string(NAME) + "_flush/" + std::to_string(numa_id) + "/" + std::to_string(cuda_id);
      benchmark::RegisterBenchmark(name.c_str(), Comm_cudaMemcpyAsync_GPUToPinned, numa_id, cuda_id, true)->SMALL_ARGS()->UseManualTime();
    }
  }
}

SCOPE_AFTER_INIT(registerer, NAME);
