#include "Clerror.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#ifdef __APPLE__
#include "OpenCL/cl.h"
#else
#include "CL/cl.h"
#endif

#define CHECK(desc, error) do { \
    if (err == CL_SUCCESS) { break; } \
    printf("Error %d %s -> %s\n", __LINE__, (desc), Clerror_str(error)); \
    abort(); \
} while (0)

#define CHECKED(expr) do { \
    int err; (expr); \
    CHECK(#expr, err); \
} while (0)

static void printLog(cl_program prog, cl_device_id* dev) {
    size_t len;
    char buffer[2048];
    //printf("Error: Failed to build program executable! %d\n", err);
    clGetProgramBuildInfo(prog, *dev, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
    printf("%s\n", buffer);
}

static cl_program compileAndLink(
    cl_context ctx,
    cl_device_id* dev,
    const char* options,
    const char* linkOptions,
    uint fileCount,
    char** fileNames
) {
    cl_program* programs = malloc(sizeof(cl_program*) * fileCount);
    assert(programs);

    char _buf[128];
    for (uint i = 0; i < fileCount; i++) {
        snprintf(_buf, 128, "#include \"%s\"", fileNames[i]);
        const char* buf = &_buf[0];
        CHECKED(programs[i] = clCreateProgramWithSource(ctx, 1, &buf, NULL, &err));
        int err = clCompileProgram(programs[i], 1, dev, options, 0, NULL, NULL, NULL, NULL);
        if (err != CL_SUCCESS) {
            printf("Failed to compile %s:\n", fileNames[i]);
            printLog(programs[i], dev);
            exit(1);
        }
    }
    int err = CL_SUCCESS;
    cl_program out = clLinkProgram(ctx, 1, dev, linkOptions, fileCount, programs, NULL, NULL, &err);
    if (err) {
        printf("Failed to link program:\n");
        printLog(out, dev);
        exit(1);
    }
    for (uint i = 0; i < fileCount; i++) {
        clReleaseProgram(programs[i]);
    }
    free(programs);
    return out;
}

typedef struct Context_s {
    cl_platform_id platform;
    cl_device_id dev;
    cl_context clCtx;
    cl_command_queue mainQ;
    cl_program prog;
} Context_t;

static void teardown(Context_t* ctx) {
    clReleaseProgram(ctx->prog);
    clReleaseCommandQueue(ctx->mainQ);
    clReleaseContext(ctx->clCtx);
    clReleaseDevice(ctx->dev);
    free(ctx);
}

static Context_t* setup() {
    Context_t* ctx = calloc(sizeof(Context_t), 1);
    assert(ctx);

    CHECKED(err = clGetPlatformIDs(1, &ctx->platform, NULL));
    CHECKED(err = clGetDeviceIDs(ctx->platform, CL_DEVICE_TYPE_ALL, 1, &ctx->dev, NULL));
    CHECKED(ctx->clCtx = clCreateContext(0, 1, &ctx->dev, NULL, NULL, &err));
    CHECKED(ctx->mainQ = clCreateCommandQueue(ctx->clCtx, ctx->dev, 0, &err));


    const char* buf = "#include \"main.cl\"";
    CHECKED(ctx->prog = clCreateProgramWithSource(ctx->clCtx, 1, &buf, NULL, &err));
    for (int err = clBuildProgram(ctx->prog, 1, &ctx->dev, "-cl-std=CL2.0", NULL, NULL); err; err = 0) {
        printf("Failed to build %s\n", Clerror_str(err));
        printLog(ctx->prog, &ctx->dev);
        exit(100);
    }

    cl_kernel cl_main;
    CHECKED(cl_main = clCreateKernel(ctx->prog, "cl_main", &err));

    cl_mem mem;
    CHECKED(mem = clCreateBuffer(ctx->clCtx, CL_MEM_READ_WRITE, 1, NULL, &err));
    CHECKED(err = clSetKernelArg(cl_main, 0, sizeof(cl_mem), &mem));

    size_t globalWorkers = 1;
    CHECKED(err = clEnqueueNDRangeKernel(
        ctx->mainQ, cl_main, 1, NULL, &globalWorkers, NULL, 0, NULL, NULL));

    clFinish(ctx->mainQ);

    clReleaseKernel(cl_main);

    return ctx;
}

int main(int argc, char** argv)
{
    Context_t* ctx = setup();
    teardown(ctx);
}
