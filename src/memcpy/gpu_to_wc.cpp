#include <cassert>
#include <cuda_runtime.h>

#include "scope/init/flags.hpp"
#include "scope/init/init.hpp"
#include "scope/utils/utils.hpp"

#include "args.hpp"

#define NAME "Comm/Memcpy/GPUToWC"

#define OR_SKIP(stmt, msg) \
  if (PRINT_IF_ERROR(stmt)) { \
    state.SkipWithError(msg); \
    return; \
  }

static void Comm_Memcpy_GPUToWC(benchmark::State &state) {

  if (!has_cuda) {
    state.SkipWithError(NAME " no CUDA device found");
    return;
  }

  const auto bytes = 1ULL << static_cast<size_t>(state.range(0));
  char *src        = nullptr;
  char *dst        = nullptr;

  const int cuda_id = FLAG(cuda_device_ids)[0];
  OR_SKIP(utils::cuda_reset_device(cuda_id), NAME " failed to reset device");
  OR_SKIP(cudaSetDevice(cuda_id), NAME " failed to set CUDA device");
  OR_SKIP(cudaMalloc(&src, bytes), NAME " failed to perform cudaMalloc");
  defer(cudaFree(src));

  OR_SKIP(cudaHostAlloc(&dst, bytes, cudaHostAllocWriteCombined), NAME " failed to allocate dst");
  defer(cudaFreeHost(dst));

  OR_SKIP(cudaMemset(src, 0, bytes), NAME " failed to perform cudaMemset");

  cudaEvent_t start, stop;
  OR_SKIP(cudaEventCreate(&start), NAME " failed to create event");
  OR_SKIP(cudaEventCreate(&stop) , NAME " failed to create event");

  for (auto _ : state) {
    OR_SKIP(cudaEventRecord(start, NULL), NAME " failed to record start event");
    OR_SKIP(cudaMemcpyAsync(dst, src, bytes, cudaMemcpyDeviceToHost), NAME " failed to memcpy");
    OR_SKIP(cudaEventRecord(stop, NULL), NAME " failed to record stop event");
    OR_SKIP(cudaEventSynchronize(stop), NAME " failed to event synchronize");

    float msecTotal = 0.0f;
    OR_SKIP(cudaEventElapsedTime(&msecTotal, start, stop), NAME "  failed to get elapsed time");
    state.SetIterationTime(msecTotal / 1000);
  }
  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(bytes));
  state.counters["bytes"] =  bytes;
  state.counters["cuda_id"] = cuda_id;
}

BENCHMARK(Comm_Memcpy_GPUToWC)->SMALL_ARGS()->UseManualTime();
