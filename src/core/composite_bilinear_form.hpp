#ifndef _COMPOSITE_BILINEAR_FORM_H_
#define _COMPOSITE_BILINEAR_FORM_H_

#include <spikes/timer.hpp>
#include <iostream>

template<typename te_cfe_type, typename tr_cfe_type>
class bilinear_form<composite_finite_element_space<te_cfe_type>,
		    composite_finite_element_space<tr_cfe_type> > {
public:
  using bilinear_form_type = bilinear_form<composite_finite_element_space<te_cfe_type>,
					   composite_finite_element_space<tr_cfe_type> >;

  using test_cfes_type = composite_finite_element_space<te_cfe_type>;
  using trial_cfes_type = composite_finite_element_space<tr_cfe_type>;

  using test_cfe_type = typename test_cfes_type::cfe_type;
  using trial_cfe_type = typename trial_cfes_type::cfe_type;

  using test_fe_list = typename test_cfe_type::fe_list;
  using trial_fe_list = typename trial_cfe_type::fe_list;
  using fe_list = cat_list_t<typename test_cfe_type::fe_list,
			     typename test_cfe_type::fe_list>;
  using unique_fe_list = unique_t<fe_list>;
  using fe_cell_type = typename get_element_at_t<0, unique_fe_list>::cell_type;

  // both cfe should have the same number of components
  static const std::size_t n_test_component = test_cfe_type::n_component;
  static const std::size_t n_trial_component = trial_cfe_type::n_component;


  bilinear_form(const test_cfes_type& te_cfes,
		const trial_cfes_type& tr_cfes)
    : test_cfes(te_cfes), trial_cfes(tr_cfes),
      a(te_cfes.get_total_dof_number(),
	tr_cfes.get_total_dof_number()),
      constraint_number(0) {
    std::size_t test_global_dof_number[n_test_component];
    fill_array_with_return_values<std::size_t,
				  get_dof_number_impl<test_cfes_type>,
				  0,
				  n_test_component>::template fill<const test_cfes_type&>(&test_global_dof_number[0],
											  test_cfes);
    std::size_t trial_global_dof_number[n_trial_component];
    fill_array_with_return_values<std::size_t,
				  get_dof_number_impl<trial_cfes_type>,
				  0,
				  n_trial_component>::template fill<const trial_cfes_type&>(&trial_global_dof_number[0],
											    trial_cfes);

    test_global_dof_offset = std::vector<std::size_t>(n_test_component, 0ul);
    std::partial_sum(test_global_dof_number,
		     test_global_dof_number + n_test_component - 1,
		     test_global_dof_offset.begin() + 1);

    trial_global_dof_offset = std::vector<std::size_t>(n_trial_component, 0ul);
    std::partial_sum(trial_global_dof_number,
		     trial_global_dof_number + n_trial_component - 1,
		     trial_global_dof_offset.begin() + 1);

    clear();
  }


  template<typename B_INFO>
  struct evaluate_block {
    using T = get_element_at_t<0, B_INFO>;
    typedef typename T::quadrature_type quadrature_type;

    static const std::size_t m = get_element_at_t<1, B_INFO>::value;
    static const std::size_t n = get_element_at_t<2, B_INFO>::value;

    using test_fe_type = get_element_at_t<m, typename test_cfe_type::fe_list>;
    using trial_fe_type = get_element_at_t<n, typename trial_cfe_type::fe_list>;

    static void call(bilinear_form_type& bilinear_form,
		     const get_element_at_t<0, B_INFO>& integration_proxy,
		     const std::size_t k,
		     const array<double>& omega,
		     const array<double>& xq,
		     const array<double>& xq_hat,
		     const fe_value_manager<unique_fe_list>& fe_values,
		     const fe_value_manager<unique_fe_list>& fe_zvalues) {
      const double* psi_phi[n_test_component + n_trial_component];

      const std::size_t n_q(quadrature_type::n_point);
      const std::size_t n_test_dof(test_fe_type::n_dof_per_element);
      const std::size_t n_trial_dof(trial_fe_type::n_dof_per_element);

      // evaluate the weak form
      const double volume(integration_proxy.m.get_cell_volume(k));
      for (unsigned int q(0); q < n_q; ++q) {
        integration_proxy.f.prepare(k, &xq.at(q, 0), &xq_hat.at(q, 0));
            
      for (unsigned int i(0); i < n_test_dof; ++i) {
	for (unsigned int j(0); j < n_trial_dof; ++j) {
	  double a_el(0.0);
	    select_function_valuation<test_fe_list, m, unique_fe_list>(psi_phi, q, i,
								       fe_values, fe_zvalues);
	    select_function_valuation<trial_fe_list, n, unique_fe_list>(psi_phi + n_test_component, q, j,
									fe_values, fe_zvalues);

	    a_el += volume * omega.at(q)
	      * (expression_call_wrapper<0, n_test_component + n_trial_component>
		 ::call(integration_proxy.f, psi_phi,
                        k, &xq.at(q, 0), &xq_hat.at(q, 0)));
	  bilinear_form.accumulate_in_block<m, n>(bilinear_form.test_cfes.
						  template get_dof<m>(integration_proxy.
								      get_global_cell_id(k), i),
						  bilinear_form.trial_cfes.
						  template get_dof<n>(integration_proxy.
								      get_global_cell_id(k), j), a_el);
        }
	}
      }
    }
  };


  template<typename T>
  void operator+=(const T& integration_proxy) {
    static_assert(T::form_type::rank == 2, "bilinear_form expects rank-2 expression.");

    typedef typename T::quadrature_type quadrature_type;
    typedef typename T::cell_type cell_type;
    using form_type = typename T::form_type;

    // m is the mesh over which we integrate
    const auto& m(integration_proxy.m);

    // prepare the quadrature weights
    const std::size_t n_q(quadrature_type::n_point);
    array<double> omega{n_q};
    omega.set_data(&quadrature_type::w[0]);

    // storage for the quadrature points;
    array<double> xq_hat{n_q, fe_cell_type::n_dimension};
    array<double> xq{n_q, fe_cell_type::n_dimension};

    // storage for the point-wise basis function evaluation
    fe_value_manager<unique_fe_list> fe_values(n_q), fe_zvalues(n_q);
    fe_zvalues.clear();

    if (T::point_set_number == 1) {
      xq_hat = integration_proxy.get_quadrature_points(0);
      fe_values.set_points(xq_hat);
    }
    
    for (unsigned int k(0); k < m.get_cell_number(); ++k) {
      // prepare the quadrature points
      if (T::point_set_number > 1) {
	xq_hat = integration_proxy.get_quadrature_points(k);
	fe_values.set_points(xq_hat);
      }

      if (form_type::require_space_coordinates)
	cell_type::map_points_to_space_coordinates(xq, m.get_vertices(),
						   m.get_cells(),
						   k, xq_hat);

      // prepare the basis function values
      if (form_type::differential_order > 0) {
	const array<double>& jmt(m.get_jmt(k));
	fe_values.prepare(jmt);
      }

      /*
       *  Compile time loop over all the blocks
       */
      using test_blocks_il = make_integral_list_t<std::size_t, n_test_component>;
      using trial_blocks_il = make_integral_list_t<std::size_t, n_trial_component>;
      using block_list = tensor_product_of_lists_t<test_blocks_il, trial_blocks_il>;
      using block_info = append_to_each_element_t<T, block_list>;

      call_for_each<evaluate_block, block_info>::call(*this, integration_proxy,
						      k,
						      omega,
						      xq, xq_hat,
						      fe_values, fe_zvalues);
    }
  }

  template<std::size_t n>
  expression<form<n, 1, 0> > get_test_function() const {
    return form<n, 1, 0>();
  }

  template<std::size_t n>
  expression<form<n + n_test_component, 2, 0> > get_trial_function() const {
    return form<n + n_test_component, 2, 0>();
  }


  template<typename IC>
  struct handle_dirichlet_dof_values {
    static const std::size_t m = IC::value;
    static void call(const bilinear_form_type& bilinear_form, array<double>& f) {
      for (const auto& i: bilinear_form.trial_cfes.template get_dirichlet_dof<m>()) {
	const auto x(bilinear_form.trial_cfes.template get_dof_space_coordinate<m>(i));
	f.at(bilinear_form.trial_global_dof_offset[m] + i) = bilinear_form.trial_cfes.template boundary_value<m>(&x.at(0, 0));
      }
    }
  };


  typename trial_cfes_type::element solve(const linear_form<test_cfes_type>& form) const {
    array<double> f(form.get_coefficients());
    call_for_each<handle_dirichlet_dof_values, make_integral_list_t<std::size_t, n_test_component> >::call(*this, f);

    linear_solver s;
    auto petsc_gmres_ilu(s.get_solver(solver::petsc,
				      method::gmres,
				      preconditioner::ilu));
    petsc_gmres_ilu->set_size(test_cfes.get_total_dof_number());

    if (true) {
      // Convert to CRS format
      std::vector<int>
	row(a.get_equation_number() + 1),
	col(a.get_elements().size());
      std::vector<double>
	val(a.get_elements().size());

      std::size_t row_id(0), val_id(0);
      row[0] = row_id;
      for (const auto& v: a.get_elements()) {
	while (row_id < v.first.first) {
	  ++row_id;
	  row[row_id] = val_id;
	}
	col[val_id] = v.first.second;
	val[val_id] = v.second;
	++val_id;
      }
      row.back() = val_id;

      // count the number of non-zero per row
      std::vector<int> nnz(a.get_equation_number());
      for (std::size_t n(0); n < a.get_equation_number(); ++n) {
	nnz[n] = row[n + 1] - row[n];
      }


      petsc_gmres_ilu->preallocate(&nnz[0]);

      if(true) {
	// Assemble line by line
	for (std::size_t row_id(0); row_id < row.size() - 1; ++row_id) {
	  if (row[row_id + 1] > row[row_id])
	    petsc_gmres_ilu->add_row(row_id,
				     nnz[row_id],
				     &col[row[row_id]],
				     &val[row[row_id]]);
	}
      } else {
	// Assemble element by element
	for (const auto& v: a.get_elements())
	  petsc_gmres_ilu->add_value(v.first.first, v.first.second, v.second);
      }
    }

    petsc_gmres_ilu->assemble();
    //petsc_gmres_ilu->show();

    typename trial_cfes_type::element result(trial_cfes, (petsc_gmres_ilu->solve(f)));
    delete petsc_gmres_ilu; petsc_gmres_ilu = nullptr;
    return result;
  }


  template<typename IC>
  struct handle_dirichlet_dof_equations {
    static const std::size_t m = IC::value;
    static void call(const bilinear_form_type& bilinear_form, sparse_linear_system& a) {
      for (const auto& i: bilinear_form.test_cfes.template get_dirichlet_dof<m>()) {
	//std::cout << "bc block " << m << ", dof " << i << ", global dof " << bilinear_form.test_global_dof_offset[m] + i << std::endl;
	a.accumulate(bilinear_form.test_global_dof_offset[m] + i,
		     bilinear_form.test_global_dof_offset[m] + i,
		     1.0);
      }
    }
  };


  void clear() {
    a.clear();
    // we need to specify the equation for the dirichlet dof
    call_for_each<handle_dirichlet_dof_equations, make_integral_list_t<std::size_t, n_test_component> >::call(*this, a);
  }

  void show(std::ostream& stream) {
    a.show(stream);
  }

  void export_data(std::ostream& stream) const {
    a.export_data(stream);
  }

  class constraint_handle;

  constraint_handle new_constraint();
  void assemble_constraint(const constraint_handle& l_1, const constraint_handle& l_2, double value);
  
private:
  const test_cfes_type& test_cfes;
  const trial_cfes_type& trial_cfes;

  std::vector<std::size_t> test_global_dof_offset;
  std::vector<std::size_t> trial_global_dof_offset;

  sparse_linear_system a;

  std::size_t constraint_number;

  template<std::size_t m, std::size_t n>
  void accumulate_in_block(std::size_t i, std::size_t j, double value) {
    if (trial_cfes.template get_dirichlet_dof<m>().count(i) == 0)
      a.accumulate(i + test_global_dof_offset[m],
		   j + trial_global_dof_offset[n],
		   value);
  }

  void accumulate(std::size_t i, std::size_t j, double value) {
    a.accumulate(i, j, value);
  }
};


template<typename test_cfe_type, typename trial_cfe_type>
class bilinear_form<composite_finite_element_space<test_cfe_type>,
                    composite_finite_element_space<trial_cfe_type>>::constraint_handle {
  using bilinear_form_type = bilinear_form<composite_finite_element_space<test_cfe_type>,
                                           composite_finite_element_space<trial_cfe_type> >;
  bilinear_form_type& b_form;
  std::size_t constraint_id;
  
public:
  constraint_handle(bilinear_form_type& f, std::size_t constraint_id)
    : b_form(f), constraint_id(constraint_id) {}

  std::size_t get_id() const { return constraint_id; }


  /*
   *  B_INFO is a type list of two elements:
   *  B_INFO = type_list<T, integral_constant<std::size_t, i> >,
   *  where T is the integration_proxy, and i is the block id being assembled.
   */
  template<typename B_INFO>
  struct evaluate_trial_block {
    using T = get_element_at_t<0, B_INFO>;
    typedef typename T::quadrature_type quadrature_type;

    static const std::size_t m = get_element_at_t<1, B_INFO>::value;

    using trial_fe_type = get_element_at_t<m, typename trial_cfe_type::fe_list>;
    using bilinear_form_type = bilinear_form<composite_finite_element_space<trial_cfe_type>,
                                             composite_finite_element_space<test_cfe_type> >;

    static void call(bilinear_form_type& bilinear_form,
                     const get_element_at_t<0, B_INFO>& integration_proxy,
                     const std::size_t constraint_id,
                     const std::size_t k,
                     const array<double>& omega,
                     const array<double>& xq,
                     const array<double>& xq_hat,
                     const fe_value_manager<unique_fe_list>& fe_values,
                     const fe_value_manager<unique_fe_list>& fe_zvalues) {
      const double* psi[n_test_component];

      const std::size_t n_q(quadrature_type::n_point);
      const std::size_t n_trial_dof(trial_fe_type::n_dof_per_element);

      const double volume(integration_proxy.m.get_cell_volume(k));
      for (unsigned int q(0); q < n_q; ++q) {
        integration_proxy.f.prepare(k, &xq.at(q, 0), &xq_hat.at(q, 0));
        for (unsigned int i(0); i < n_trial_dof; ++i) {
          double rhs_el(0.0);
          select_function_valuation<trial_fe_list, m, unique_fe_list>(psi, q, i, fe_values, fe_zvalues);

          rhs_el += volume * omega.at(q)
            * expression_call_wrapper<0, n_trial_component>::call(integration_proxy.f, psi,
                                                                  k, &xq.at(q, 0), &xq_hat.at(q, 0));
          bilinear_form.accumulate(bilinear_form.test_fel.get_total_dof_number() + constraint_id,
                                   bilinear_form.trial_global_dof_offset[m]
                                   + bilinear_form.trial_cfes.template get_dof<m>(integration_proxy.get_global_cell_id(k), i),
                                   rhs_el);
        }
      }

    }
  };

  
  template<typename T>
  void operator-=(const T& integration_proxy) {
    using quadrature_type = typename T::quadrature_type;
    using cell_type = typename T::cell_type;
    using form_type = typename T::form_type;

    const auto& m(integration_proxy.m);

    const std::size_t n_q(quadrature_type::n_point);
    array<double> omega{n_q};
    omega.set_data(&quadrature_type::w[0]);

    array<double> xq_hat{n_q, fe_cell_type::n_dimension};
    array<double> xq{n_q, fe_cell_type::n_dimension};

    fe_value_manager<unique_fe_list> fe_values(n_q), fe_zvalues(n_q);
    fe_zvalues.clear();

    if(T::point_set_number == 1) {
      xq_hat = integration_proxy.get_quadrature_points(0);
      fe_values.set_points(xq_hat);
    }

    for (unsigned int k(0); k < m.get_cell_number(); ++k) {
      if (T::point_set_number > 1) {
        xq_hat = integration_proxy.get_quadrature_points(k);
        fe_values.set_points(xq_hat);
      }

      if (form_type::require_space_coordinates)
        cell_type::map_points_to_space_coordinates(xq, m.get_vertices(), m.get_cells(), k, xq_hat);

      if (form_type::differential_order > 0) {
        const array<double>& jmt(m.get_jmt(k));
        fe_values.prepare(jmt);
      }

      using trial_blocks_il = wrap_t<type_list, make_integral_list_t<std::size_t, n_trial_component> >;
      using block_info = append_to_each_element_t<T, trial_blocks_il>;

      call_for_each<evaluate_trial_block, block_info>::call(*this, integration_proxy, constraint_id,
                                                            k, omega, xq, xq_hat, fe_values, fe_zvalues);
    }
      
  }

  template<typename B_INFO>
  struct evaluate_test_block {
    using T = get_element_at_t<0, B_INFO>;
    typedef typename T::quadrature_type quadrature_type;

    static const std::size_t m = get_element_at_t<1, B_INFO>::value;

    using test_fe_type = get_element_at_t<m, typename test_cfe_type::fe_list>;
    using bilinear_form_type = bilinear_form<composite_finite_element_space<trial_cfe_type>,
                                             composite_finite_element_space<test_cfe_type> >;

    static void call(bilinear_form_type& bilinear_form,
                     const get_element_at_t<0, B_INFO>& integration_proxy,
                     const std::size_t constraint_id,
                     const std::size_t k,
                     const array<double>& omega,
                     const array<double>& xq,
                     const array<double>& xq_hat,
                     const fe_value_manager<unique_fe_list>& fe_values,
                     const fe_value_manager<unique_fe_list>& fe_zvalues) {
      const double* psi[n_test_component];

      const std::size_t n_q(quadrature_type::n_point);
      const std::size_t n_test_dof(test_fe_type::n_dof_per_element);

      const double volume(integration_proxy.m.get_cell_volume(k));
      for (unsigned int q(0); q < n_q; ++q) {
        integration_proxy.f.prepare(k, &xq.at(q, 0), &xq_hat.at(q, 0));
        for (unsigned int i(0); i < n_test_dof; ++i) {
          double rhs_el(0.0);
          select_function_valuation<test_fe_list, m, unique_fe_list>(psi, q, i, fe_values, fe_zvalues);

          rhs_el += volume * omega.at(q)
            * expression_call_wrapper<0, n_test_component>::call(integration_proxy.f, psi,
                                                                  k, &xq.at(q, 0), &xq_hat.at(q, 0));
          bilinear_form.accumulate(bilinear_form.test_global_dof_offset[m]
                                   + bilinear_form.test_cfes.template get_dof<m>(integration_proxy.get_global_cell_id(k), i),
                                   bilinear_form.trial_fes.get_total_dof_number() + constraint_id,
                                   rhs_el);
        }
      }

    }
  };

  
  template<typename T>
  void operator|=(const T& integration_proxy) {
    using quadrature_type = typename T::quadrature_type;
    using cell_type = typename T::cell_type;
    using form_type = typename T::form_type;

    const auto& m(integration_proxy.m);

    const std::size_t n_q(quadrature_type::n_point);
    array<double> omega{n_q};
    omega.set_data(&quadrature_type::w[0]);

    array<double> xq_hat{n_q, fe_cell_type::n_dimension};
    array<double> xq{n_q, fe_cell_type::n_dimension};

    fe_value_manager<unique_fe_list> fe_values(n_q), fe_zvalues(n_q);
    fe_zvalues.clear();

    if(T::point_set_number == 1) {
      xq_hat = integration_proxy.get_quadrature_points(0);
      fe_values.set_points(xq_hat);
    }

    for (unsigned int k(0); k < m.get_cell_number(); ++k) {
      if (T::point_set_number > 1) {
        xq_hat = integration_proxy.get_quadrature_points(k);
        fe_values.set_points(xq_hat);
      }

      if (form_type::require_space_coordinates)
        cell_type::map_points_to_space_coordinates(xq, m.get_vertices(), m.get_cells(), k, xq_hat);

      if (form_type::differential_order > 0) {
        const array<double>& jmt(m.get_jmt(k));
        fe_values.prepare(jmt);
      }

      using test_blocks_il = wrap_t<type_list, make_integral_list_t<std::size_t, n_test_component> >;
      using block_info = append_to_each_element_t<T, test_blocks_il>;

      call_for_each<evaluate_test_block, block_info>::call(*this, integration_proxy, constraint_id,
                                                           k, omega, xq, xq_hat, fe_values, fe_zvalues);
    }
  }
};

template<typename test_cfe_type, typename trial_cfe_type>
typename bilinear_form<composite_finite_element_space<test_cfe_type>,
                       composite_finite_element_space<trial_cfe_type> >::constraint_handle
bilinear_form<composite_finite_element_space<test_cfe_type>,
              composite_finite_element_space<trial_cfe_type> >::new_constraint() {
  constraint_number += 1;
  a.set_sizes(test_cfes.get_total_dof_number() + constraint_number,
	      trial_cfes.get_total_dof_number() + constraint_number);
  return constraint_handle(*this, constraint_number - 1);
}


template<typename test_cfe_type, typename trial_cfe_type>
void bilinear_form<composite_finite_element_space<test_cfe_type>,
                   composite_finite_element_space<trial_cfe_type> >::assemble_constraint
(const constraint_handle& l_1,
 const constraint_handle& l_2,
 double value) {
  accumulate(test_cfes.get_total_dof_number() + l_1.get_id(),
	     trial_cfes.get_total_dof_number() + l_2.get_id(), value);
}

#endif /* _COMPOSITE_BILINEAR_FORM_H_ */
