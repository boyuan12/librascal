/**
 * @file   rascal/math/bessel.hh
 *
 * @author  Felix Musil <felix.musil@epfl.ch>
 * @author  Max Veit <max.veit@epfl.ch>
 *
 * @date   27 May 2019
 *
 * @brief Implementation of the modified spherical bessel of the 1st kind
 *
 * Copyright  2019  Felix Musil, Max Veit COSMO (EPFL), LAMMM (EPFL)
 *
 * Rascal is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3, or (at
 * your option) any later version.
 *
 * Rascal is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software; see the file LICENSE. If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef SRC_RASCAL_MATH_BESSEL_HH_
#define SRC_RASCAL_MATH_BESSEL_HH_

#include "rascal/math/hyp1f1.hh"
#include "rascal/math/utils.hh"

namespace rascal {
  namespace math {

    /**
     * Computes the modified spherical bessel function of the first kind (MBSF)
     * as
     * \f[
     *    f(r; x_n, a) = e^{-ar^2} e^{-ax_n^2} i_l(2*a*r*x_n)
     * \f]
     * Just call precompute() once, calc(), and use get_values() to access
     * the values
     *
     * Note that this uses the "exp-exp complete-square" optimized function,
     * where the Bessel function is multiplied by two exponentials that keep
     * it from blowing up.
     *
     * The recursion relation used here is:
     * \f[
     *    i_0(x) = sinh(x) / x
     *    i_1(x) = (x*cosh(x) - sinh(x)) / x^2
     *    i_n(x) = i_{n-2}(x) - (2n - 1)/x * i_{n-1}(x)
     * \f]
     *
     * The recursion relation to compute the gradient is:
     * \f[
     *    i'_n(x) = n i_{n-1}(x) / (2n+1) + (n + 1) i_{n+1}(x) / (2n+1)
     * \f]
     *
     * from
     * http://mathworld.wolfram.com/ModifiedSphericalBesselFunctionoftheFirstKind.html
     * // NOLINT
     */
    class ModifiedSphericalBessel {
     public:
      using ref = Eigen::Ref<const Eigen::ArrayXXd>;

      ModifiedSphericalBessel() = default;

      /**
       * Initialize arrays for the computation of
       * @param x_v      Eigen::Array of x-values (part of the argument of the
       *                 first exponential; see equation above)
       */
      void precompute(size_t l_max,
                      const Eigen::Ref<const Eigen::VectorXd> & x_v,
                      bool compute_gradients = false) {
        this->compute_gradients = compute_gradients;
        this->x_v = x_v.array();
        this->n_max = x_v.size();
        this->l_max = l_max;
        if (this->compute_gradients) {
          // to compute the gradients with the recursion formula we need one
          // extra order for the values
          this->order_max = static_cast<int>(this->l_max + 2);
          this->bessel_gradients.resize(this->n_max, this->l_max + 1);
        } else {
          this->order_max = static_cast<int>(this->l_max + 1);
        }

        // set the proper sizes
        this->bessel_values.resize(this->n_max, this->order_max);
        this->bessel_arg.resize(this->n_max);
        this->bessel_arg_i.resize(this->n_max);
        this->exp_bessel_arg.resize(this->n_max);
        this->efac.resize(this->n_max);

        // precompute for downward recursion
        this->igammas.resize(2);
        int ii{0};
        for (int order{this->order_max - 2}; order < this->order_max; ++order) {
          this->hyp1f1s.emplace_back(static_cast<double>(order + 1),
                                     static_cast<double>(2 * order + 2));
          this->igammas[ii] = 1. / std::tgamma(1.5 + order);
          ii++;
        }
      }

      /**
       * Compute the MBSFs times two exponentials that complete the square
       * using upward recursion. This is stable when 2*a*r*x_n > 50, so
       * it is useful to avoid overflow/underflow in the individual terms of f.
       *
       * @param distance (single) value of distance
       *                (second exponential argument) in the equation above
       * @param fac_a  Scaling factor for the exponential arguments
       * @param n_rows number of rows where the recursion is applicable
       *               from the bottom
       */
      void upward_recursion(double distance, double fac_a, int n_rows) {
        auto vals = this->bessel_values.bottomRows(n_rows);
        // i_0(z) = sinh(z) / z
        vals.col(0) =
            (Eigen::exp(-fac_a * (x_v.tail(n_rows) - distance).square()) -
             Eigen::exp(-fac_a * (x_v.tail(n_rows) + distance).square())) *
            0.5 * this->bessel_arg_i.tail(n_rows);
        // i_1(z) = cosh(z)/z - i_0(z)/z
        vals.col(1) =
            ((Eigen::exp(-fac_a * (x_v.tail(n_rows) - distance).square()) +
              Eigen::exp(-fac_a * (x_v.tail(n_rows) + distance).square())) *
             0.5 * this->bessel_arg_i.tail(n_rows)) -
            vals.col(0) * this->bessel_arg_i.tail(n_rows);

        for (int order{2}; order < this->order_max; ++order) {
          vals.col(order) =
              vals.col(order - 2) - vals.col(order - 1) * (2. * order - 1.) *
                                        this->bessel_arg_i.tail(n_rows);
        }
      }

      /**
       * Compute the MBSFs times two exponentials using downward recurence
       * which is stable in general but when a, r and/or x_n becomes too large
       * one of the terms of f might overflow/underflow while f is finite.
       * The recurence relation is initialized using the confluent
       * hypergeometric function.
       *
       * \f[
       *    f(r; x_n, a) = e^{-ar^2} e^{-ax_n^2} i_l(2*a*r*x_n)
       * \f]
       * using the representation of Modified Bessel function as 1F1
       * \f[
       *  i_l(x) = \exp{-x} \frac{\sqrt{\pi}}{4\Gamma{1.5 + n}}
       *            (\frac{x}{2})^{l} 1F1(l+1, 2l+2, 2x)
       * \f]
       *
       * @param distance (single) value of distance
       *                (second exponential argument) in the equation above
       * @param fac_a  Scaling factor for the exponential arguments
       * @param n_rows number of rows where the recursion is applicable
       *               from the top
       */
      void downward_recursion(double distance, double fac_a, int n_rows) {
        auto vals = this->bessel_values.topRows(n_rows);
        this->exp_bessel_arg = Eigen::exp(-this->bessel_arg.head(n_rows));
        this->efac = std::exp(-fac_a * distance * distance) *
                     Eigen::exp(-fac_a * this->x_v.head(n_rows).square());
        for (int i_order{0}; i_order < 2; ++i_order) {
          int order{this->order_max - 2 + i_order};
          auto & hyp1f1{this->hyp1f1s[i_order]};
          for (int ii{0}; ii < n_rows; ++ii) {
            vals(ii, order) =
                this->exp_bessel_arg[ii] * this->igammas[i_order] *
                math::pow(this->bessel_arg[ii] * 0.5, order) * 0.5 *
                math::SQRT_PI * hyp1f1.calc(2. * this->bessel_arg[ii]);
          }
          vals.col(order) *= this->efac;
        }

        for (int order{this->order_max - 3}; order >= 0; --order) {
          vals.col(order) =
              vals.col(order + 2) + vals.col(order + 1) * (2. * order + 3.) *
                                        this->bessel_arg_i.head(n_rows);
        }
      }

      /**
       * Compute all the MBSFs derivative for the given x-values up to the
       * given order:
       * \f[
       *    df(r; x_n, a)/dr = -2ar e^{-ar^2} e^{-ax_n^2} i_l(2*a*r*x_n) +
       *                       2ax_n e^{-ar^2} e^{-ax_n^2} i'_l(2*a*r*x_n)
       * \f]
       *
       * using the recursion relation:
       * \f[
       *    i'_0(x) = i_1(x)
       *    i'_n(x) = (n i_{n-1}(x) + (n + 1) i_{n+1}(x)) / (2n+1)
       * \f]
       *
       * Expects that values have already been computed with an additional
       * order
       */
      void gradient_recursion(double distance, double fac_a) {
        // compute 1st part
        this->bessel_gradients = this->bessel_values.leftCols(this->l_max + 1);
        this->bessel_gradients *= -2. * fac_a * distance;
        // add 2nd part
        this->efac = 2. * fac_a * this->x_v;
        this->bessel_gradients.col(0) +=
            this->efac * this->bessel_values.col(1);
        // use recurrence relationship
        for (int i_order{1}; i_order < this->order_max - 1; i_order++) {
          this->bessel_gradients.col(i_order) +=
              this->efac *
              (i_order * this->bessel_values.col(i_order - 1) +
               (i_order + 1) * this->bessel_values.col(i_order + 1)) /
              (2 * i_order + 1);
        }
      }

      /**
       * Compute all the MBSFs for the given x-values up to the given order
       *
       * The MBSFs are accurate when the expected value is > 1e-100. Below this
       * threshold the MBSFs are set to 0 because of the numerical noise
       * arising below 1e-150.
       */
      void calc(double distance, double fac_a) {
        this->bessel_arg = (2. * fac_a * distance) * this->x_v;
        this->bessel_arg_i = this->bessel_arg.inverse();

        if (this->order_max == 1) {
          // recursions are not valid for l_max==0 so direct computation
          // i_0(z) = sinh(z) / z
          this->bessel_values.col(0) =
              (Eigen::exp(-fac_a * (x_v - distance).square()) -
              Eigen::exp(-fac_a * (x_v + distance).square())) *
              0.5 * this->bessel_arg_i;
        } else {
          // for l_max > 0 downward/upward_recursion functions are applicable
          // find the index where bessel_arg is larger than 50
          // (bessel_arg is sorted by increasing order)
          int n_down{0};
          for (; n_down < this->n_max; ++n_down) {
            if (this->bessel_arg[n_down] > 50) {
              break;
            }
          }
          // apply downward recurence where bessel_arg < 50
          if (n_down > 0) {
            this->downward_recursion(distance, fac_a, n_down);
          }

          // apply upward recurence where bessel_arg > 50
          int n_up{this->n_max - n_down};
          if (n_up > 0) {
            this->upward_recursion(distance, fac_a, n_up);
          }
        }
        assert(this->bessel_values.isFinite().all());

        // Set small values to 0 because the recursion looses accuracy for very
        // small values. Also on the python side it avoids some unexpected
        // interpretation of values that are strictly speaking outside of the
        // range of double precision
        bessel_values = bessel_values.unaryExpr([](double d) {
          if (d < 1e-100) {
            return 0.;
          } else {
            return d;
          }
        });

        // compute gradients
        if (this->compute_gradients) {
          this->gradient_recursion(distance, fac_a);
          assert(this->bessel_gradients.isFinite().all());
        }
      }

      /**
       * Return a reference to the precomputed Bessel function values
       *
       * @return Eigen::Array (2-D) of Bessel function values.  The different
       *         arguments (xs) go along the rows, while the column indexes
       *         the orders (n-values).
       *         Note that a reference is returned to avoid unnecessary
       *         copies.
       */
      auto get_values() {
        if (this->compute_gradients) {
          // when the gradient are computed bessel_values has one additional
          // column that should not be returned
          return ref(bessel_values.leftCols(this->l_max + 1));
        } else {
          return ref(bessel_values);
        }
      }
      auto get_gradients() { return ref(bessel_gradients); }

      Eigen::ArrayXXd bessel_values{};
      Eigen::ArrayXXd bessel_gradients{};

      Eigen::ArrayXd bessel_arg{};
      Eigen::ArrayXd bessel_arg_i{};
      Eigen::ArrayXd exp_bessel_arg{};
      Eigen::ArrayXd x_v{};
      Eigen::ArrayXd efac{};
      std::vector<Hyp1f1> hyp1f1s{};
      Eigen::ArrayXd igammas{};

      bool compute_gradients{false};
      int order_max{};
      size_t l_max{};
      int n_max{};
    };

  }  // namespace math
}  // namespace rascal

#endif  // SRC_RASCAL_MATH_BESSEL_HH_
