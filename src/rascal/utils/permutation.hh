/**
 * @file   permutation.hh
 *
 * @author Till Junge <till.junge@epfl.ch>
 *
 * @date   08 Jan 2020
 *
 * @brief  Compile-time permutation (for evaluating symmetry functions in
 * arbitrary cluster order)
 *
 * Copyright © 2020 Till Junge
 *
 * rascal is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3, or (at
 * your option) any later version.
 *
 * rascal is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with rascal; see the file COPYING. If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#include <array>
#include <type_traits>
#ifndef SRC_RASCAL_UTILS_PERMUTATION_HH_
#define SRC_RASCAL_UTILS_PERMUTATION_HH_

namespace rascal {

  /**
   * Refers to the pattern of repeated species in the permuted cluster, as it is
   * evaluated by the symmetry function
   */
  enum class RepeatedSpecies {
    Unknown,    // has not been evaluated yet
    Not,        // all species in this cluster are unique
    All,        // all atoms in this cluster are same species
    FirstTwo,   // the first two atoms of this cluster are of same species
    SecondTwo,  // the second two atoms of this cluster are of same species
    OuterTwo    // the first and last atom in this cluster are of same species
  };

  /* ---------------------------------------------------------------------- */
  constexpr std::array<int, 3>
  triplet_representation(RepeatedSpecies rep_species) {
    switch (rep_species) {
    case RepeatedSpecies::Not: {
      return {0, 1, 2};
      break;
    }
    case RepeatedSpecies::All: {
      return {0, 0, 0};
      break;
    }
    case RepeatedSpecies::FirstTwo: {
      return {0, 0, 1};
      break;
    }
    case RepeatedSpecies::SecondTwo: {
      return {0, 1, 1};
      break;
    }
    case RepeatedSpecies::OuterTwo: {
      return {0, 1, 0};
      break;
    }
    default:
      throw std::runtime_error("Can't represent unknown repetitions");
      break;
    }
  }

  /* ---------------------------------------------------------------------- */
  constexpr bool jk_are_same_species(const RepeatedSpecies rep_species) {
    const auto trip_repr{triplet_representation(rep_species)};
    return trip_repr[1] == trip_repr[2];
  }

  constexpr RepeatedSpecies
  triplet_representation(const std::array<int, 3> rep_species) {
    if (rep_species[0] == rep_species[1]) {
      if (rep_species[1] == rep_species[2]) {
        return RepeatedSpecies::All;
      } else {
        return RepeatedSpecies::FirstTwo;
      }
    } else if (rep_species[0] == rep_species[2]) {
      return RepeatedSpecies::OuterTwo;
    } else {
      if (rep_species[1] == rep_species[2]) {
        return RepeatedSpecies::SecondTwo;
      } else {
        return RepeatedSpecies::Not;
      }
    }
  }

  // // Proposed for usage in test for iterating RepeatedSpecies
  // static constexpr RepeatedSpecies AllRepSpecies[] = {
  //     RepeatedSpecies::Unknown,   RepeatedSpecies::Not,
  //     RepeatedSpecies::All,       RepeatedSpecies::FirstTwo,
  //     RepeatedSpecies::SecondTwo, RepeatedSpecies::OuterTwo};

  constexpr size_t nb_triplet_orderings(const RepeatedSpecies rep,
                                        const bool jk_indistinguishable) {
    // A triplet needs to be evaluated in every permutation for which the
    // leading species corresponds to the symmetry function's center species

    // If a symmetry function distinguishes between triplet ijk and ikj (where j
    // and k have the same species), both triplets need to be evaluated
    int nb_evals{jk_indistinguishable ? 1 : 2};
    switch (rep) {
    case RepeatedSpecies::Not: {
      // fall-through
    }
    case RepeatedSpecies::SecondTwo: {
      return 1 * nb_evals;
      break;
    }
    case RepeatedSpecies::FirstTwo: {
      // fall-through
    }
    case RepeatedSpecies::OuterTwo: {
      return 2 * nb_evals;
      break;
    }
    case RepeatedSpecies::All: {
      return 3 * nb_evals;
      break;
    }
    default: {
      throw std::runtime_error("Unknown species repetition");
    }
    }
  }

  template <size_t Size_, size_t First, size_t Second, size_t Third = Size_ - 1>
  struct Permutation {
    static_assert((First != Second) && (Size_ > First) && (Size_ > Second) &&
                      (Size_ > Third) &&
                      ((Size_ == 2) ||
                       ((Size_ == 3) && (Second != Third) && (First != Third))),
                  "Not a valid  pair or triplet permutation");
    static constexpr int leading() { return First; }
    static constexpr int second() { return Second; }
    static constexpr size_t Size{Size_};

    /* ---------------------------------------------------------------------- */
    template <bool IsTriplet = (Size_ == 3)>
    static constexpr std::enable_if_t<IsTriplet, int> third() {
      return Third;
    }

    /* ---------------------------------------------------------------------- */
    template <typename StructureManager, typename Cluster>
    static int leading(StructureManager & manager, const Cluster & cluster) {
      return manager.get_atom_index(cluster.get_atom_tag_list(leading()));
    }

    /* ---------------------------------------------------------------------- */
    template <typename StructureManager, typename Cluster>
    static int second(StructureManager & manager, const Cluster & cluster) {
      return manager.get_atom_index(cluster.get_atom_tag_list(second()));
    }

    /* ---------------------------------------------------------------------- */
    template <class Derived, bool IsPair = (Size_ == PairOrder),
              std::enable_if_t<IsPair, int> = 0>
    static auto
    flip_direction(const Eigen::MatrixBase<Derived> & direction_vector)
        -> decltype(1 * direction_vector) {
      static_assert(Size_ == PairOrder, "IsPair is a SFINAE, don't touch");
      if (First > Second) {
        return -1 * direction_vector;
      } else {
        return 1 * direction_vector;
      }
    }

    /* ---------------------------------------------------------------------- */
    template <typename StructureManager, typename Cluster,
              bool IsTriplet = (Size_ == 3)>
    static std::enable_if_t<IsTriplet, int> third(StructureManager & manager,
                                                  const Cluster & cluster) {
      return manager.get_atom_index(cluster.get_atom_tag_list(third()));
    }

    /**
     * @tparam JK_Indistinguishable whether or not the sum of the evaluations of
     * triplet ijk and triplet ikj equals twice the evaluation of ijk
     */
    template <RepeatedSpecies RepSpecies, bool JK_Indistinguishable,
              bool CompatibilityMode>
    constexpr static std::tuple<
        std::array<std::tuple<std::array<size_t, 3>, std::array<bool, 3>>,
                   nb_triplet_orderings(RepSpecies, JK_Indistinguishable)>,
        int>
    get_triplet_orderings() {
      std::tuple<
          std::array<std::tuple<std::array<size_t, 3>, std::array<bool, 3>>,
                     nb_triplet_orderings(RepSpecies, JK_Indistinguishable)>,
          int>
          retval{};
      auto && orderings{std::get<0>(retval)};

      // A triplet with species ABB (or BBB) represents the two triplets AB₁B₂
      // and AB₂B₁ (or BₓB₁B₂ and BₓB₂B₁). This may or may not be taken into
      // account, depending on legacy mode
      constexpr bool CountSameSpecies{JK_Indistinguishable and
                                      jk_are_same_species(RepSpecies) and
                                      (not CompatibilityMode)};

      switch (RepSpecies) {
      case RepeatedSpecies::Not: {
        std::get<0>(orderings[0])[0] = leading();
        std::get<0>(orderings[0])[1] = second();
        std::get<0>(orderings[0])[2] = third();

        std::get<1>(orderings[0]) = {leading() > second(), second() > third(),
                                     third() > leading()};
        // weight (no repeated species)
        std::get<1>(retval) = 1.;
        break;
      }
      case RepeatedSpecies::SecondTwo: {
        std::get<0>(orderings[0])[0] = leading();
        std::get<0>(orderings[0])[1] = second();
        std::get<0>(orderings[0])[2] = third();

        std::get<1>(orderings[0]) = {leading() > second(), second() > third(),
                                     third() > leading()};
        // Calculation of weight
        // If j and k atom are of the same element,
        std::get<1>(retval) = CountSameSpecies ? 2 : 1;
        break;
      }
      case RepeatedSpecies::FirstTwo: {
        std::get<0>(orderings[0])[0] = leading();
        std::get<0>(orderings[0])[1] = second();
        std::get<0>(orderings[0])[2] = third();
        std::get<1>(orderings[0]) = {leading() > second(), second() > third(),
                                     third() > leading()};

        std::get<0>(orderings[1])[0] = second();
        std::get<0>(orderings[1])[1] = leading();
        std::get<0>(orderings[1])[2] = third();
        std::get<1>(orderings[1]) = {second() > leading(), leading() > third(),
                                     third() > second()};
        // weight (no repeated species)
        std::get<1>(retval) = 1.;
        break;
      }
      case RepeatedSpecies::OuterTwo: {
        std::get<0>(orderings[0])[0] = leading();
        std::get<0>(orderings[0])[1] = second();
        std::get<0>(orderings[0])[2] = third();
        std::get<1>(orderings[0]) = {leading() > second(), second() > third(),
                                     third() > leading()};

        std::get<0>(orderings[1])[0] = third();
        std::get<0>(orderings[1])[1] = second();
        std::get<0>(orderings[1])[2] = leading();
        std::get<1>(orderings[1]) = {third() > second(), second() > leading(),
                                     leading() > third()};
        // weight (no repeated species)
        std::get<1>(retval) = 1.;
        break;
      }
      case RepeatedSpecies::All: {
        std::get<0>(orderings[0])[0] = leading();
        std::get<0>(orderings[0])[1] = second();
        std::get<0>(orderings[0])[2] = third();
        std::get<1>(orderings[0]) = {leading() > second(), second() > third(),
                                     third() > leading()};

        std::get<0>(orderings[1])[0] = second();
        std::get<0>(orderings[1])[1] = third();
        std::get<0>(orderings[1])[2] = leading();
        std::get<1>(orderings[1]) = {second() > third(), third() > leading(),
                                     leading() > second()};

        std::get<0>(orderings[2])[0] = third();
        std::get<0>(orderings[2])[1] = leading();
        std::get<0>(orderings[2])[2] = second();
        std::get<1>(orderings[2]) = {third() > leading(), leading() > second(),
                                     second() > third()};
        // Calculation of weight
        // If j and k atom are of the same element,
        std::get<1>(retval) = CountSameSpecies ? 2 : 1;
        break;
      }
      default:
        throw std::runtime_error("Unknown species repetition");
        break;
      }

      static_assert(JK_Indistinguishable,
                    "not implemented for distinguishable j,k atoms");
      return retval;
    }

    /* ---------------------------------------------------------------------- */
    template <bool IsTriplet = (Size_ == 3)>
    constexpr static std::enable_if_t<IsTriplet, RepeatedSpecies>
    permute(const RepeatedSpecies rep_species) {
      static_assert(IsTriplet == (Size_ == 3),
                    "IsTriplet is a SFINAE parameter, don't touch it");
      const std::array<int, 3> rep_species_arr{
          triplet_representation(rep_species)};

      constexpr std::array<size_t, 3> tmp{First, Second, Third};
      const auto permuted_arr{apply_ordering(rep_species_arr, tmp)};

      return triplet_representation(permuted_arr);
    }

    /* ---------------------------------------------------------------------- */
    template <typename T>
    constexpr static std::array<T, 3>
    apply_ordering(const std::array<T, 3> values,
                   const std::array<size_t, 3> ordering) {
      return std::array<T, 3>{values[ordering[0]], values[ordering[1]],
                              values[ordering[2]]};

      //      std::array<double, 3> ret_val{} : ret_val[0] =
      //      values[ordering[0]];
    }

    /**
     * returns and array of boolean indicating for each pair of this cluster
     * whether it corresponds to a really existing pair in a minimal neighbour
     * list (false) or to and inverted (e.g. ji, rather than ij) pair (true)
     */
    constexpr static std::array<bool, nb_distances(Size)> pair_inversion() {
      std::array<bool, nb_distances(Size)> ret_val{};
      switch (Size) {
      case PairOrder: {
        ret_val[0] = First > Second;  // 0,1 = 0 ; 1,0 = 1
        break;
      }
      case TripletOrder: {
        ret_val[0] = First > Second;  // true if first pair is 10, 21, or 20
        ret_val[1] = Second > Third;  // true if second pair is 10, 21, or 20
        ret_val[2] = Third > First;   // true if third pair is 10, 21, or 20
        break;
      }
      default: {
        break;
      }
      }
      return ret_val;
    }
  };  // Permutation
  template <size_t Size_, size_t First, size_t Second, size_t Third>
  constexpr size_t Permutation<Size_, First, Second, Third>::Size;

}  // namespace rascal

#endif  // SRC_RASCAL_UTILS_PERMUTATION_HH_