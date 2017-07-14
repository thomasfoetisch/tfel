
#include "../src/fes.hpp"
#include "../src/fe.hpp"
#include "../src/mesh.hpp"
#include "../src/cell.hpp"
#include "../src/timer.hpp"

void build_fes(std::size_t n) {
  const mesh<cell::triangle> m(gen_square_mesh(1.0, 1.0, n, n));
  volatile finite_element_space<finite_element::triangle_lagrange_p1> fes(m);
}

int main(int argc, char *argv[]) {
  std::vector<std::size_t> ns{10, 20, 30, 40, 50,
                              60, 70, 80, 90, 100,
      110, 120, 130, 140, 150,
      160, 170, 180, 190, 200,
      210, 220, 230, 240, 250,
      300, 350, 400, 450, 500,
      600, 700, 800, 1000};
  for (auto n: ns) {
    timer t;
    build_fes(n);
    std::cout << n << " " << t.tic() << std::endl;
  }
  
  return 0;
}