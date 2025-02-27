// RUN: %clangxx -fsycl -fsycl-targets=%sycl_triple %s -o %t.out
// RUN: %t.out

// UNSUPPORTED: cuda

#include <sycl/sycl.hpp>

#include <cmath>

class Kernel1Name;
class Kernel2Name;

struct TestStruct {
  int a;
  int b;
};

const static sycl::specialization_id<int> SpecConst1{42};
const static sycl::specialization_id<int> SpecConst2{42};
const static sycl::specialization_id<TestStruct> SpecConst3{TestStruct{42, 42}};
const static sycl::specialization_id<short> SpecConst4{42};

int main() {
  sycl::queue Q;

  // No support for host device so far
  if (Q.is_host())
    return 0;

  // The code is needed to just have device images in the executable
  if (0) {
    Q.submit([](sycl::handler &CGH) { CGH.single_task<Kernel1Name>([] {}); });
    Q.submit([](sycl::handler &CGH) { CGH.single_task<Kernel2Name>([] {}); });
  }

  const sycl::context Ctx = Q.get_context();
  const sycl::device Dev = Q.get_device();

  sycl::kernel_bundle KernelBundle =
      sycl::get_kernel_bundle<sycl::bundle_state::input>(Ctx, {Dev});

  assert(KernelBundle.contains_specialization_constants() == true);
  assert(KernelBundle.has_specialization_constant<SpecConst1>() == false);
  assert(KernelBundle.has_specialization_constant<SpecConst2>() == true);
  KernelBundle.set_specialization_constant<SpecConst2>(1);
  {
    auto ExecBundle = sycl::build(KernelBundle);
    sycl::buffer<int, 1> Buf{sycl::range{1}};
    Q.submit([&](sycl::handler &CGH) {
      CGH.use_kernel_bundle(ExecBundle);
      auto Acc = Buf.get_access<sycl::access::mode::read_write>(CGH);
      CGH.single_task<class Kernel3Name>([=](sycl::kernel_handler KH) {
        Acc[0] = KH.get_specialization_constant<SpecConst2>();
      });
    });
    auto Acc = Buf.get_access<sycl::access::mode::read>();
    assert(Acc[0] == 1);
  }

  {
    sycl::buffer<TestStruct, 1> Buf{sycl::range{1}};
    Q.submit([&](sycl::handler &CGH) {
      auto Acc = Buf.get_access<sycl::access::mode::read_write>(CGH);
      CGH.set_specialization_constant<SpecConst3>(TestStruct{1, 2});
      const auto SC = CGH.get_specialization_constant<SpecConst4>();
      assert(SC == 42);
      CGH.single_task<class Kernel4Name>([=](sycl::kernel_handler KH) {
        Acc[0] = KH.get_specialization_constant<SpecConst3>();
      });
    });
    auto Acc = Buf.get_access<sycl::access::mode::read>();
    assert(Acc[0].a == 1 && Acc[0].b == 2);
  }

  return 0;
}
