/**
 * @file   bind_py_neighbour_manager.cc
 *
 * @author Felix Musil <felix.musil@epfl.ch>
 *
 * @date   9 Mai 2018
 *
 * @brief  File for binding the Neighbour Managers
 *
 * Copyright © 2018  Felix Musil, COSMO (EPFL), LAMMM (EPFL)
 *
 * Rascal is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3, or (at
 * your option) any later version.
 *
 * Rascal is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Emacs; see the file COPYING. If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <pybind11/pybind11.h>
#include "structure_managers/structure_manager_cell.hh"
#include "structure_managers/structure_manager_base.hh"
#include <pybind11/eigen.h>
#include <pybind11/stl.h>
#include <Eigen/Dense>
#include <basic_types.hh>

using namespace rascal;
namespace py=pybind11;

template<size_t Level>
decltype(auto) add_cluster(py::module & m) {
  using ClusterRef = StructureManager<StructureManagerCell>::ClusterRef<Level>;
  py::class_<ClusterRef> py_cluster (m, (Level == 1) ? "Cell.Center" : "Cell.Neighbour");
  py_cluster.def_property_readonly("atom_index", & ClusterRef::get_atom_index)
    .def_property_readonly("atom_type", & ClusterRef::get_atom_type)
    .def_property_readonly("index", & ClusterRef::get_index)
    .def_property_readonly("size", & ClusterRef::size)
    .def_property("position",
		  & ClusterRef::get_position,
		  [] (ClusterRef & cluster, py::EigenDRef<Eigen::MatrixXd> mat) {
		    cluster.get_position() = mat;
		  });
    return py_cluster;
}

void add_manager_cell(py::module& m){
  m.doc()  = "binding for the Neighbourhood Manager Linked Cell" ;
  py::class_<StructureManager<StructureManagerCell>>(m, "StructureManagerBase_Cell")
    .def(py::init<>());

  py::class_<StructureManagerCell,StructureManager<StructureManagerCell>>(m, "StructureManagerCell")
    .def(py::init<>())
    .def("update",&StructureManagerCell::update)
    //.def("build",[]())
    .def("__iter__",
	 [](StructureManager<StructureManagerCell> &v) {
	   return py::make_iterator(v.begin(),v.end());
	 }, py::keep_alive<0, 1>()); /* Keep vector alive while iterator is used */

  add_cluster<1>(m)
    .def("__iter__",
	 [](StructureManager<StructureManagerCell>::ClusterRef<1> &v) {
	   return py::make_iterator(v.begin(),v.end());
	 }, py::keep_alive<0, 1>()); /* Keep vector alive while iterator is used */

  add_cluster<2>(m);


  // // TODO: return value policy?
  // // TODO: cleaner: def_property w/ setter
  //   using ClusterRef2 = StructureManagerBase<StructureManagerCell>::ClusterRef<2>;
  // py::class_<StructureManagerBase<StructureManagerCell>::ClusterRef<2>>(m, "Cell.Neighbour")
  //   .def("get_atom_index",&StructureManagerBase<StructureManagerCell>::ClusterRef<2>::get_atom_index)
  //   .def("get_atom_type",&StructureManagerBase<StructureManagerCell>::ClusterRef<2>::get_atom_type)
  //   .def("get_index",&StructureManagerBase<StructureManagerCell>::ClusterRef<2>::get_index)
  //   .def("get_size",&StructureManagerBase<StructureManagerCell>::ClusterRef<2>::size)
  //   //.def("get_atom_shift",&StructureManagerBase<StructureManagerCell>::ClusterRef<2>::get_atom_shift)
  //   .def("get_position",&StructureManagerBase<StructureManagerCell>::ClusterRef<2>::get_position);
}
