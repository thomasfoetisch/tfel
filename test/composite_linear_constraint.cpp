
#include <utility>

#include "../src/tfel.hpp"


/*
 *  Zero dirichlet boundary conditions
 */
double bc(const double* x) {
  return 0.0;
}


/*
 *  Forces with zero integrals over m
 */
double f0(const double* x) { return x[0] - 0.5; }
double f1(const double* x) { return x[1] - 0.5; }
double f2(const double* x) { return x[0] + x[1] - 1.0; }


/*
 *  Solve 3 independent laplacian with pure (dirichlet) neumann boundary conditions
 */
int main() {
  try {
    const std::size_t n(100);
    
    using cell_type = cell::triangle;
    using fe_type = cell_type::fe::lagrange_p1;
    using cfe_type = composite_finite_element<fe_type, fe_type, fe_type>;
    using cfes_type = composite_finite_element_space<cfe_type>;
    using quad_type = quad::triangle::qf5pT;
      
    fe_mesh<cell_type> m(gen_square_mesh(1.0, 1.0, n, n));
    submesh<cell_type> dm(m.get_boundary_submesh());
    
    cfes_type cfes(m);
    //cfes.set_dirichlet_boundary_condition<0>(dm, bc);
    //cfes.set_dirichlet_boundary_condition<1>(dm, bc);
    //cfes.set_dirichlet_boundary_condition<2>(dm, bc);

    bilinear_form<cfes_type, cfes_type> a(cfes, cfes, 3, 3); {
      auto u0(a.get_trial_function<0>());
      auto u1(a.get_trial_function<1>());
      auto u2(a.get_trial_function<2>());

      auto v0(a.get_test_function<0>());
      auto v1(a.get_test_function<1>());
      auto v2(a.get_test_function<2>());


      a += integrate<quad_type>(d<1>(u0) * d<1>(v0) + d<2>(u0) * d<2>(v0) +
                                d<1>(u1) * d<1>(v1) + d<2>(u1) * d<2>(v1) +
                                d<1>(u2) * d<1>(v2) + d<2>(u2) * d<2>(v2),
                                m);

      a.algebraic_trial_block(0) += integrate<quad_type>(u0, m);
      a.algebraic_trial_block(0) += integrate<quad_type>(v0, m);
      a.algebraic_block(0, 0) = 0.0;

      a.algebraic_trial_block(1) += integrate<quad_type>(u1, m);
      a.algebraic_trial_block(1) += integrate<quad_type>(v1, m);
      a.algebraic_block(1, 1) = 0.0;

      a.algebraic_trial_block(2) += integrate<quad_type>(u2, m);
      a.algebraic_trial_block(2) += integrate<quad_type>(v2, m);
      a.algebraic_block(2, 2) = 0.0;
    }

    linear_form<cfes_type> f(cfes); {
      auto v0(f.get_test_function<0>());
      auto v1(f.get_test_function<1>());
      auto v2(f.get_test_function<2>());

      f += integrate<quad_type>(f0 * v0 +
                                f1 * v1 +
                                f2 * v2,
                                m);

      f.algebraic_equation_value(0) = 0.0;
      f.algebraic_equation_value(1) = 0.0;
      f.algebraic_equation_value(2) = 0.0;
    }

    dictionary p(dictionary()
                 .set("maxits",  2000u)
                 .set("restart", 1000u)
                 .set("rtol",    1.e-8)
                 .set("abstol",  1.e-50)
                 .set("dtol",    1.e20)
                 .set("ilufill", 2u));
    solver::petsc::gmres_ilu s(p);
    
    auto x(a.solve(f, s));

    auto u0(x.get_component<0>());
    auto u1(x.get_component<1>());
    auto u2(x.get_component<2>());

    exporter::ensight6("composite_linear_constraint",
                       to_mesh_vertex_data<fe_type>(u0), "u0",
                       to_mesh_vertex_data<fe_type>(u1), "u1",
                       to_mesh_vertex_data<fe_type>(u2), "u2");
  }
  catch (const std::string& e) {
    std::cout << e << std::endl;
  }
  
  return 0;
}
