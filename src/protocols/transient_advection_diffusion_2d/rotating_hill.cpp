#include "../../formulations/transient_advection_diffusion_2d.hpp"


double bell(double r_sqr) {
  const double epsilon(1.0e-5);
  if (std::abs(r_sqr) < 1.0 - epsilon) {
    return std::exp(1.0 - 1.0 / (1.0 - r_sqr));
  } else {
    return 0.0;
  }
}

double shift_scale_bell(const double* x, double width, double x_0, double x_1) {
  return bell((std::pow(x[0] - x_0, 2) +
	       std::pow(x[1] - x_1, 2)) / (width * width));
}


struct vortex {
  static double b_0(const double* x) {
    return - (x[1] - 0.5) * M_PI / 2.0;
  }

  static double b_1(const double* x){
    return (x[0] - 0.5) * M_PI / 2.0;
  }
};


struct kim_moin {
  static double b_0(const double* x) {
    return std::sin(x[0] * M_PI) * std::cos(x[1] * M_PI);
  }

  static double b_1(const double* x) {
    return - std::cos(x[0] * M_PI) * std::sin(x[1] * M_PI);
  }
};


double u_bc(const double* x) {
  return 1.0;
}

double exact_solution(double time, const double* x) {
  double xp[] = {std::cos(-M_PI/2.0 * time) * (x[0] - 0.5) - std::sin(-M_PI/2.0 * time) * (x[1] - 0.5) + 0.5,
		 std::sin(-M_PI/2.0 * time) * (x[0] - 0.5) + std::cos(-M_PI/2.0 * time) * (x[1] - 0.5) + 0.5};
  return shift_scale_bell(xp, 1.0 / 8.0, 0.5, 0.75);
}

double sqr(double x) { return x * x; }

double error(std::size_t M, std::size_t N);

int main(int argc, char *argv[]) {
  std::vector<std::size_t>
    Ms{64, 181, 512, 1449, 4096},
    Ns{16,  32,  64,  128,  256};

  std::cout << "# delta_t h l2_error" << std::endl;
  for (std::size_t i(0); i < Ms.size(); ++i) {
    std::size_t N(Ns[i]), M(Ms[i]);
    const double err(error(M, N));
    std::cout << 1.0 / M << " " << 1.0 / N << " " << err << std::endl;
  }
}

double error(std::size_t M, std::size_t N) {
  const double diffusivity(0.0);
  const double t_end(1.0);
  const double delta_t(t_end / M);
    
  
  using cell_type = cell::triangle;
  mesh<cell_type> m(gen_square_mesh(1.0, 1.0, N, N));
  submesh<cell_type> dm(m.get_boundary_submesh());

  {
    submesh<cell_type> inflow_boundary(dm.inflow_boundary([&](const double* x) -> array<double> {
      array<double> b{2};
      b.at(0) = vortex::b_0(x);
      b.at(1) = vortex::b_1(x);
      return b;
    }));
  }

  submesh<cell_type> inflow_boundary(dm.query_elements([](const double* x) {
	return x[1] < 0.0001;
      }));
  
  using fe_type = finite_element::triangle_lagrange_p1;
  transient_advection_diffusion<fe_type> tad(m, delta_t, diffusivity);
  tad.set_boundary_value(u_bc);
  tad.set_advection_velocity(vortex::b_0, vortex::b_1);
  tad.set_initial_condition([](const double* x) {
      return shift_scale_bell(x, 1.0 / 8.0, 0.5, 0.75);
    });


  /*exporter::ensight6_transient<fe_type>
    ens("transient_advection_diffusion_rotating_hill",
    m, "solution");*/

  double time(0.0);
  //ens.export_time_step(time, tad.get_solution());
  for (std::size_t k(0); k < M; ++k) {
    std::cerr << "step " << k << std::endl;
    time += delta_t;
    tad.step();
    //ens.export_time_step(time, tad.get_solution());
  }


  finite_element_space<fe_type> fes(m);
  exporter::ensight6("exact_sol"
		     , projector::lagrange<fe_type>([=](const double* x) -> double { return exact_solution(time, x) ;}, fes)
		     , "solution");
  
  return std::sqrt(integrate<quad::triangle::qf5pT>(compose(sqr,
							    (make_expr(std::bind(exact_solution, time, std::placeholders::_1))
							     - make_expr<fe_type>(tad.get_solution()))), m));
}
