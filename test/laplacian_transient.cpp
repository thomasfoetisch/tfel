
#include <cmath>

#include "../src/mesh.hpp"
#include "../src/fe.hpp"
#include "../src/fes.hpp"
#include "../src/quadrature.hpp"
#include "../src/projector.hpp"
#include "../src/export.hpp"
#include "../src/timer.hpp"
#include "../src/expression.hpp"

double bell(double x) {
  const double epsilon(1e-5);
  if (std::abs(x) < 1.0 - epsilon) {
    return std::exp(1.0 - 1.0 / (1.0 - x * x));
  } else {
    return 0.0;
  }
}

double shift_scale_bell(double x, double width, double x_0) {
  return bell((x - x_0) / width);
}


double velocity_x(const double* x) { return -x[1]; }
double velocity_y(const double* x) { return  x[0]; }


double initial_condition(const double* x) {
  return std::sin(M_PI * x[0]) * std::sin(M_PI * x[1]);
  //const double x_0(0.5), x_1(0.5);
  //const double r(std::sqrt(std::pow(x[0] - x_0, 2) + std::pow(x[1] - x_1, 2)));
  //return shift_scale_bell(r, 0.25, 0.0);
}

double source(const double* x) {
  //return (-1.0 + 2.0 * M_PI * M_PI) * initial_condition(x);
  return - 4.0;
}

double bc(const double* x) {
  return x[0] * x[0] + x[1] * x[1];
}

int main(int argc, char *argv[]) {

  typedef cell::triangle cell_type;
  typedef finite_element::triangle_lagrange_p1 fe_type;
  typedef finite_element_space<fe_type> fes_type;
  typedef typename finite_element_space<fe_type>::element element_type;
  typedef quad::triangle::qf1pTlump q_type;
  
  const std::size_t n(25);
  mesh<cell_type> m(gen_square_mesh(1.0, 1.0, n, n));
  submesh<cell_type> dm(m.get_boundary_submesh());

  fes_type fes(m, dm, bc);
  const element_type u_init(projector::l2<fe_type,
			    q_type>(initial_condition,
						   fes));
  exporter::ensight6<fe_type>("initial_condition", u_init, "u_init");


  const std::size_t M(20);
  const double delta_t(0.5 / M);
  const double diffusivity(1.0);

  bilinear_form<fes_type, fes_type> a(fes, fes); {
    const auto u(a.get_trial_function());
    const auto v(a.get_test_function());
    a += integrate<q_type>(diffusivity * (d<1>(u) * d<1>(v) + d<2>(u) * d<2>(v)), m);
    a += integrate<q_type>((0.0 / delta_t) * u * v, m);
  }

  element_type u(u_init);  
  for (std::size_t k(0); k < M; ++k) {
    std::cout << "step " << k << std::endl;
    linear_form<fes_type> f(fes); {
      const auto v(f.get_test_function());
      f += integrate<q_type>((0.0 / delta_t) * make_expr<fe_type>(u) * v
			     + (source * v), m);
    }
    u = a.solve(f);
  }

  exporter::ensight6<fe_type>("solution", u, "u");
  
  return 0;
}