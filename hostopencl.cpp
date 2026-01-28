#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <vector>
#include <CL/cl.h>

using namespace std;

void matrixclockupdate(std::vector<int> m1, std::vector<int> m0, std::vector<int>& m2, int size, int position){
    const int n =size;
    const int pos =position;
    const int total=n*n;

    const char *matrix_program = R"CLC(
    __kernel void matrixupdate(__global const int* a,
                        __global const int* b,
                        __global  int* c, int pos, int n)
    {
        int gid = get_global_id(0);
        
        int p1=a[gid];
        int p2=b[gid];
        c[gid]= (p1>p2)? p1 : p2;  
        
        if((gid/n)==(pos-1)){
            if((gid%n)==(pos-1)){
                c[gid]+=1;
            }else{
                c[gid]=a[(gid%n)*(n+1)];
            }
        }
    }
    )CLC";

    // create the openCL context on a GPU device
    cl_int err;
    cl_context context = clCreateContextFromType(
        0, CL_DEVICE_TYPE_GPU, NULL, NULL, &err);
    if(err != CL_SUCCESS){
        cout << "Failed to create context\n";
        return;
    }    

    // get the list of GPU devices associated with context
    size_t cb;
    err = clGetContextInfo(context, CL_CONTEXT_DEVICES, 0, NULL, &cb);
    if(err != CL_SUCCESS){
        cout << "Failed get context\n";
        return;
    }

    vector <cl_device_id> devices(cb/ sizeof(cl_device_id));
    clGetContextInfo(context, CL_CONTEXT_DEVICES, cb, devices.data(), NULL);

    // create a command-queue
    cl_command_queue cmd_queue = 
        clCreateCommandQueue(context, devices[0], 0, NULL);
    
    // create input vectors
    vector<int> h_a= m1;
    vector<int> h_b= m0;
    vector<int>& h_c= m2; //reference to new matrix

    // allocate the buffer memory objects
    cl_mem d_a = clCreateBuffer(
            context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
            sizeof(cl_int) * n*n, h_a.data(),&err);//h_a.data(),&err);
    if(err != CL_SUCCESS) {
         cout << "Failed to create buffer d_a\n"; 
         return; 
        }

    cl_mem d_b = clCreateBuffer(
            context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 
            sizeof(cl_int) * n*n, h_b.data(),&err);//h_b.data(), &err);
    if(err != CL_SUCCESS) {
        cout << "Failed to create buffer d_b\n"; 
        return; 
    }

    cl_mem d_c = clCreateBuffer(
            context, CL_MEM_WRITE_ONLY, sizeof(cl_int) * n*n, NULL,NULL);

    // create the program
    cl_program p1 = 
        clCreateProgramWithSource(context, 1, &matrix_program, NULL, NULL);

    // build the program 1
    err = clBuildProgram(p1, 0, NULL, NULL, NULL, NULL);
    if (err != CL_SUCCESS){
        size_t len;
        char buffer[2048];
        clGetProgramBuildInfo(p1, devices[0], CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
        cout << buffer << "\n";
        return;
    }

    // create the kernel
    cl_kernel kernel = clCreateKernel(p1, "matrixupdate", NULL);
 
    // set the args values for kernel 1
    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_a);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_b);
    err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &d_c);
    err |= clSetKernelArg(kernel, 3, sizeof(int), &pos);
    err |= clSetKernelArg(kernel, 4, sizeof(int), &n);
    if (err != CL_SUCCESS){
        cout << "Failed to create Kernel" << "\n";
        return;
    }

    // set work-item dimensions
    size_t global_work_size[1] = {total};

    //execute kernel1
    err= clEnqueueNDRangeKernel(cmd_queue, kernel,1,NULL,
    global_work_size,NULL,0,NULL,NULL);
    if(err!= CL_SUCCESS){
        cout << "Failed to enqueue kernel";
        return;
    }

    err= clEnqueueReadBuffer(cmd_queue,d_c,CL_TRUE,0,
    sizeof(int) * total,h_c.data(),0,NULL,NULL);
    if(err!= CL_SUCCESS){
        cout << "Failed to enqueue";
        return;
    }

   return;
}
