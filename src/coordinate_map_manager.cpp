/*
 * Copyright (c) 2020 NVIDIA CORPORATION.
 * Copyright (c) Chris Choy (chrischoy@ai.stanford.edu).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Please cite "4D Spatio-Temporal ConvNets: Minkowski Convolutional Neural
 * Networks", CVPR'19 (https://arxiv.org/abs/1904.08755) if you use any part
 * of the code.
 */
#include "coordinate_map_manager.hpp"
#include "coordinate_map_key.hpp"
#include "errors.hpp"
#include "kernel_region.hpp"
#include "utils.hpp"

#include <pybind11/pybind11.h>
#include <string>

namespace py = pybind11;

namespace minkowski {

namespace detail {

default_types::stride_type zeros(size_t const len) { return _fill_vec<0>(len); }

default_types::stride_type ones(size_t const len) { return _fill_vec<1>(len); }

} // namespace detail
/*

template <typename MapType>
vector<at::Tensor>
CoordsManager<MapType>::getCoordsMap(py::object py_in_coords_key,
                                     py::object py_out_coords_key) const {
  CoordsKey *p_in_coords_key = py_in_coords_key.cast<CoordsKey *>();
  CoordsKey *p_out_coords_key = py_out_coords_key.cast<CoordsKey *>();
  const uint64_t in_coords_key = p_in_coords_key->getKey();
  const uint64_t out_coords_key = p_out_coords_key->getKey();

  const auto in_map_iter = coords_maps.find(in_coords_key);
  const auto out_map_iter = coords_maps.find(out_coords_key);

  ASSERT(in_map_iter != coords_maps.end(), "Input coords not found at",
         to_string(in_coords_key));
  ASSERT(out_map_iter != coords_maps.end(), "Output coords not found at",
         to_string(out_coords_key));

  const auto &out_tensor_strides = p_out_coords_key->getTensorStride();
  const auto in_out =
      in_map_iter->second.stride_map(out_map_iter->second, out_tensor_strides);

  const auto &ins = in_out.first;
  const auto &outs = in_out.second;
  // All size
  const auto N = std::accumulate(ins.begin(), ins.end(), 0,
                                 [](size_t curr_sum, const vector<int> &map) {
                                   return curr_sum + map.size();
                                 });

  at::Tensor in_out_1 =
      torch::empty({N}, torch::TensorOptions().dtype(torch::kInt64));
  at::Tensor in_out_2 =
      torch::empty({N}, torch::TensorOptions().dtype(torch::kInt64));

  auto a_in_out_1 = in_out_1.accessor<long int, 1>();
  auto a_in_out_2 = in_out_2.accessor<long int, 1>();

  size_t curr_it = 0;
  for (const auto &in : ins)
    for (const auto i : in)
      a_in_out_1[curr_it++] = i;

  curr_it = 0;
  for (const auto &out : outs)
    for (const auto o : out)
      a_in_out_2[curr_it++] = o;

  return {in_out_1, in_out_2};
}

// Generate and return the ins -> out map.
template <typename MapType>
pair<vector<at::Tensor>, vector<at::Tensor>>
CoordsManager<MapType>::getUnionMap(vector<py::object> py_in_coords_keys,
                                    py::object py_out_coords_key) {

  // all exception handling will be done inside the following
  const InOutMapsRefPair<int> in_outs =
      getUnionInOutMaps(py_in_coords_keys, py_out_coords_key);
  const auto &ins = in_outs.first;
  const auto &outs = in_outs.second;

  // Size of the in out maps
  const auto N = ins.size();

  // Return torch tensor
  vector<at::Tensor> th_ins;
  vector<at::Tensor> th_outs;
  for (size_t i = 0; i < N; ++i) {
    at::Tensor th_in = torch::empty(
        {(long)ins[i].size()}, torch::TensorOptions().dtype(torch::kInt64));
    at::Tensor th_out = torch::empty(
        {(long)outs[i].size()}, torch::TensorOptions().dtype(torch::kInt64));

    copy_types(ins[i], th_in);
    copy_types(outs[i], th_out);

    th_ins.push_back(move(th_in));
    th_outs.push_back(move(th_out));
  }

  return make_pair(th_ins, th_outs);
}

*/

/*******************************
 * Initialization
 *******************************/

namespace detail {

template <typename coordinate_type, typename coordinate_field_type>
struct insert_and_map_functor<coordinate_type, coordinate_field_type,
                              std::allocator, CoordinateMapCPU> {

  std::pair<at::Tensor, at::Tensor>
  operator()(coordinate_map_key_type &map_key, at::Tensor const &th_coordinate,
             CoordinateMapManager<coordinate_type, coordinate_field_type,
                                  std::allocator, CoordinateMapCPU> &manager) {
    LOG_DEBUG("initialize_and_map");
    uint32_t const N = th_coordinate.size(0);
    uint32_t const coordinate_size = th_coordinate.size(1);
    coordinate_type *p_coordinate = th_coordinate.data_ptr<coordinate_type>();
    auto map = CoordinateMapCPU<coordinate_type, std::allocator>(
        N, coordinate_size, map_key.first);
    auto map_inverse_map = map.template insert_and_map<true>(
        p_coordinate, p_coordinate + N * coordinate_size);
    LOG_DEBUG("mapping size:", map_inverse_map.first.size());

    // insert moves map
    manager.insert(map_key, map);

    auto const &mapping = map_inverse_map.first;
    auto const &inverse_mapping = map_inverse_map.second;

    // return tensors
    at::Tensor th_mapping = torch::empty(
        {(int64_t)mapping.size()},
        torch::TensorOptions().requires_grad(false).dtype(torch::kInt64));
    at::Tensor th_inverse_mapping = torch::empty(
        {(int64_t)inverse_mapping.size()},
        torch::TensorOptions().requires_grad(false).dtype(torch::kInt64));

    // copy_n to int to long
    int64_t *p_mapping = th_mapping.data_ptr<int64_t>();
    for (default_types::index_type i = 0; i < mapping.size(); ++i) {
      p_mapping[i] = mapping[i];
    }

    int64_t *p_inverse_mapping = th_inverse_mapping.data_ptr<int64_t>();
    for (default_types::index_type i = 0; i < inverse_mapping.size(); ++i) {
      p_inverse_mapping[i] = inverse_mapping[i];
    }

    return std::make_pair(std::move(th_mapping), std::move(th_inverse_mapping));
  }
};

template <typename coordinate_type, typename coordinate_field_type>
struct insert_field_functor<
    coordinate_type, coordinate_field_type, std::allocator, CoordinateMapCPU,
    CoordinateFieldMapCPU<coordinate_field_type, std::allocator>> {

  void
  operator()(coordinate_map_key_type &map_key, at::Tensor const &th_coordinate,
             CoordinateMapManager<coordinate_type, coordinate_field_type,
                                  std::allocator, CoordinateMapCPU> &manager) {
    LOG_DEBUG("insert field");
    uint32_t const N = th_coordinate.size(0);
    uint32_t const coordinate_size = th_coordinate.size(1);
    coordinate_field_type *p_coordinate =
        th_coordinate.data_ptr<coordinate_field_type>();
    auto map = CoordinateFieldMapCPU<coordinate_field_type, std::allocator>(
        N, coordinate_size, map_key.first);
    map.insert(p_coordinate, p_coordinate + N * coordinate_size);

    LOG_DEBUG("insert map with tensor_stride", map_key.first);
    manager.insert_field_map(map_key, map);
  }
};

} // namespace detail

/*
 * coords: coordinates in IntTensor
 * mapping: output mapping in IntTensor
 * tensor_strides: current tensor strides this coords will be initializeds
 * force_creation: even when there's a duplicate coords with the same tensor
 *                 strides.
 * force_remap: if there's duplicate coords, remap
 * allow_duplicate_coords: create map when there are duplicates in the
 * coordinates
 */
template <typename coordinate_type, typename coordinate_field_type,
          template <typename C> class TemplatedAllocator,
          template <typename T, template <typename Q> class A>
          class CoordinateMapType>
py::object CoordinateMapManager<coordinate_type, coordinate_field_type,
                                TemplatedAllocator, CoordinateMapType>::
    insert_field(at::Tensor const &coordinates,
                 default_types::stride_type const tensor_stride,
                 std::string const string_id) {

  torch::TensorArg arg_coordinate(coordinates, "coordinates", 0);
  torch::CheckedFrom c = "initialize";
  torch::checkContiguous(c, arg_coordinate);

  // must match coordinate_type
  torch::checkScalarType(c, arg_coordinate, torch::kFloat);
  torch::checkBackend(c, arg_coordinate.tensor,
                      detail::is_cpu_coordinate_map<CoordinateMapType>::value
                          ? torch::Backend::CPU
                          : torch::Backend::CUDA);
  torch::checkDim(c, arg_coordinate, 2);

  auto const coordinate_size = (index_type)coordinates.size(1);

  // Basic assertions
  ASSERT(coordinate_size - 1 == tensor_stride.size(),
         "The coordinate dimension (coordinate_size - 1):", coordinate_size - 1,
         " must match the size of tensor stride:", ArrToString(tensor_stride));

  // generate the map_key
  coordinate_map_key_type map_key = std::make_pair(tensor_stride, string_id);
  if (m_field_coordinates.find(map_key) != m_field_coordinates.end()) {
    WARNING(true, "CoordinateMapKey collision detected:", map_key,
            "generating new string id.");
    map_key = get_random_string_id(tensor_stride, string_id);
  }

  LOG_DEBUG("initializing a map with tensor stride:", map_key.first,
            "string id:", map_key.second);
  // Create the concurrent coords map
  detail::insert_field_functor<coordinate_type, coordinate_field_type,
                               TemplatedAllocator, CoordinateMapType,
                               field_map_type>()(map_key, coordinates, *this);

  py::object py_key = py::cast(new CoordinateMapKey(coordinate_size, map_key));

  return py_key;
}

/*
 * coords: coordinates in IntTensor
 * mapping: output mapping in IntTensor
 * tensor_strides: current tensor strides this coords will be initializeds
 * force_creation: even when there's a duplicate coords with the same tensor
 *                 strides.
 * force_remap: if there's duplicate coords, remap
 * allow_duplicate_coords: create map when there are duplicates in the
 * coordinates
 */
template <typename coordinate_type, typename coordinate_field_type,
          template <typename C> class TemplatedAllocator,
          template <typename T, template <typename Q> class A>
          class CoordinateMapType>
std::pair<py::object, std::pair<at::Tensor, at::Tensor>>
CoordinateMapManager<coordinate_type, coordinate_field_type, TemplatedAllocator,
                     CoordinateMapType>::
    insert_and_map(at::Tensor const &coordinate,
                   default_types::stride_type const tensor_stride,
                   std::string const string_id) {

  torch::TensorArg arg_coordinate(coordinate, "coordinates", 0);
  torch::CheckedFrom c = "initialize";
  torch::checkContiguous(c, arg_coordinate);
  // must match coordinate_type
  torch::checkScalarType(c, arg_coordinate, torch::kInt);
  torch::checkBackend(c, arg_coordinate.tensor,
                      detail::is_cpu_coordinate_map<CoordinateMapType>::value
                          ? torch::Backend::CPU
                          : torch::Backend::CUDA);
  torch::checkDim(c, arg_coordinate, 2);

  auto const coordinate_size = (index_type)coordinate.size(1);

  // Basic assertions
  ASSERT(coordinate_size - 1 == tensor_stride.size(),
         "The coordinate dimension (coordinate_size - 1):", coordinate_size - 1,
         " must match the size of tensor stride:", ArrToString(tensor_stride));

  // generate the map_key
  coordinate_map_key_type map_key = std::make_pair(tensor_stride, string_id);
  if (m_coordinate_maps.find(map_key) != m_coordinate_maps.end()) {
    WARNING(true, "CoordinateMapKey collision detected:", map_key,
            "generating new string id.");
    map_key = get_random_string_id(tensor_stride, string_id);
  }

  LOG_DEBUG("initializing a map with tensor stride:", map_key.first,
            "string id:", map_key.second);
  // Create the concurrent coords map
  auto const map_inverse_map =
      detail::insert_and_map_functor<coordinate_type, coordinate_field_type,
                                     TemplatedAllocator, CoordinateMapType>()(
          map_key, coordinate, *this);

  py::object py_key = py::cast(new CoordinateMapKey(coordinate_size, map_key));

  return std::make_pair(py_key, std::move(map_inverse_map));
}

// stride
template <typename coordinate_type, typename coordinate_field_type,
          template <typename C> class TemplatedAllocator,
          template <typename T, template <typename Q> class A>
          class CoordinateMapType>
std::pair<coordinate_map_key_type, bool> CoordinateMapManager<
    coordinate_type, coordinate_field_type, TemplatedAllocator,
    CoordinateMapType>::stride(coordinate_map_key_type const &in_map_key,
                               stride_type const &kernel_stride) {
  ASSERT(exists(in_map_key), ERROR_MAP_NOT_FOUND);
  // check if the key exists.
  LOG_DEBUG("In tensor stride:", in_map_key.first,
            "kernel stride:", kernel_stride);
  coordinate_map_key_type out_map_key(
      detail::stride_tensor_stride(in_map_key.first, kernel_stride, false), "");
  LOG_DEBUG("Out stride map key:", out_map_key);
  bool const exists_out_map = exists(out_map_key);
  if (!exists_out_map) {
    // operator[] required mapped_type(), which is not defined.
    // ASSERTION already checked that in_map_key exists.
    map_type const &in_map = m_coordinate_maps.find(in_map_key)->second;
    map_type out_map = in_map.stride(kernel_stride);
    insert(out_map_key, out_map);
  }
  // (key, new map generated flag)
  return std::make_pair(out_map_key, !exists_out_map);
}

template <typename coordinate_type, typename coordinate_field_type,
          template <typename C> class TemplatedAllocator,
          template <typename T, template <typename Q> class A>
          class CoordinateMapType>
std::pair<coordinate_map_key_type, bool>
CoordinateMapManager<coordinate_type, coordinate_field_type, TemplatedAllocator,
                     CoordinateMapType>::
    stride_region(coordinate_map_key_type const &in_map_key,
                  cpu_kernel_region<coordinate_type> &kernel,
                  stride_type const &out_tensor_stride,
                  bool const expand_coordinates) {
  ASSERT(exists(in_map_key), ERROR_MAP_NOT_FOUND);
  LOG_DEBUG("stride_region");
  // kernel.tensor_stride must be set to out tensor stride.
  // stride_type out_tensor_stride{kernel.tensor_stride(),
  //                               kernel.tensor_stride() +
  //                                   kernel.coordinate_size() - 1};

  // check if the key exists.
  coordinate_map_key_type out_map_key(out_tensor_stride, "");
  bool const exists_out_map = exists(out_map_key);
  if (!exists_out_map || expand_coordinates) {
    LOG_DEBUG("Create a new stride region map for tensor_stride:",
              out_tensor_stride);
    map_type const &in_map = m_coordinate_maps.find(in_map_key)->second;
    map_type out_map = in_map.stride_region(kernel, out_tensor_stride);
    if (exists_out_map) {
      LOG_DEBUG("coordinate map exists for tensor_stride:", out_tensor_stride);
      out_map_key = get_random_string_id(out_tensor_stride, "");
      LOG_DEBUG("created a random key:", out_map_key);
    }
    insert(out_map_key, out_map);
  }
  // (key, new map generated flag)
  return std::make_pair(out_map_key, !exists_out_map || expand_coordinates);
}

template <typename coordinate_type, typename coordinate_field_type,
          template <typename C> class TemplatedAllocator,
          template <typename T, template <typename Q> class A>
          class CoordinateMapType>
std::pair<coordinate_map_key_type, bool>
CoordinateMapManager<coordinate_type, coordinate_field_type, TemplatedAllocator,
                     CoordinateMapType>::origin() {
  ASSERT(m_coordinate_maps.size() > 0, "No coordinate map found");
  // check if the key exists.
  map_type const &random_map = m_coordinate_maps.begin()->second;
  stride_type origin_tensor_stride(random_map.coordinate_size() - 1);
  std::for_each(origin_tensor_stride.begin(), origin_tensor_stride.end(),
                [](auto &i) { i = 0; });
  LOG_DEBUG("origin tensor stride:", origin_tensor_stride);

  coordinate_map_key_type origin_map_key(origin_tensor_stride, "");
  bool const exists_origin_map = exists(origin_map_key);
  if (!exists_origin_map) {
    LOG_DEBUG("origin coordinate map not found");
    map_type const *p_min_coordinate_map{nullptr};
    size_type min_size = std::numeric_limits<size_type>::max();
    for (auto map_it = m_coordinate_maps.begin();
         map_it != m_coordinate_maps.end(); ++map_it) {
      if (min_size > map_it->second.size()) {
        p_min_coordinate_map = &(map_it->second);
      }
    }

    if (p_min_coordinate_map != nullptr) {
      map_type origin_map = p_min_coordinate_map->origin();
      LOG_DEBUG("origin map with size:", origin_map.size(), " inserted");
      insert(origin_map_key, origin_map);
    } else {
      ASSERT(false, "Invalid origin map");
    }
  }

  // (key, new map generated flag)
  return std::make_pair(origin_map_key, !exists_origin_map);
}

template <typename coordinate_type, typename coordinate_field_type,
          template <typename C> class TemplatedAllocator,
          template <typename T, template <typename Q> class A>
          class CoordinateMapType>
coordinate_map_key_type
CoordinateMapManager<coordinate_type, coordinate_field_type, TemplatedAllocator,
                     CoordinateMapType>::prune(coordinate_map_key_type const
                                                   &in_key,
                                               bool const *keep_begin,
                                               bool const *keep_end) {

  ASSERT(exists(in_key), "In map doesn't exist");

  // create a coordinate_map_key
  coordinate_map_key_type map_key = std::make_pair(in_key.first, "");
  if (m_coordinate_maps.find(map_key) != m_coordinate_maps.end()) {
    map_key = get_random_string_id(map_key.first, "");
  }

  auto const map_it = m_coordinate_maps.find(in_key);
  ASSERT(map_it != m_coordinate_maps.end(), ERROR_MAP_NOT_FOUND);

  map_type pruned_map = map_it->second.prune(keep_begin, keep_end);
  LOG_DEBUG("pruned map with size:", pruned_map.size(), " inserted");
  insert(map_key, pruned_map);

  return map_key;
}

// Kernel map

namespace detail {

template <typename coordinate_type>
struct kernel_map_functor<coordinate_type, std::allocator, CoordinateMapCPU,
                          cpu_kernel_map> {

  cpu_kernel_map
  operator()(CoordinateMapCPU<coordinate_type, std::allocator> const &in_map,
             CoordinateMapCPU<coordinate_type, std::allocator> const &out_map,
             CUDAKernelMapMode::Mode kernel_map_mode,
             cpu_kernel_region<coordinate_type> &kernel) {
    return in_map.kernel_map(out_map, kernel);
  }
};

template <typename coordinate_type>
struct stride_map_functor<coordinate_type, std::allocator, CoordinateMapCPU,
                          cpu_kernel_map> {

  cpu_kernel_map
  operator()(CoordinateMapCPU<coordinate_type, std::allocator> const &in_map,
             CoordinateMapCPU<coordinate_type, std::allocator> const &out_map,
             default_types::stride_type const &stride) {
    return in_map.stride_map(out_map, stride);
  }
};

// a partial specialization functor for kernel map in/out swap
template <> struct swap_in_out_map_functor<cpu_kernel_map> {

  cpu_kernel_map operator()(cpu_kernel_map const &kernel_map) {
    return std::make_pair(kernel_map.second, kernel_map.first);
  }
};

} // namespace detail

/*
 * Given tensor_stride_src and tensor_stride_dst, find the respective coord_maps
 * and return the indices of the coord_map_ind in coord_map_dst
 */
template <typename coordinate_type, typename coordinate_field_type,
          template <typename C> class TemplatedAllocator,
          template <typename T, template <typename Q> class A>
          class CoordinateMapType>
typename CoordinateMapManager<coordinate_type, coordinate_field_type,
                              TemplatedAllocator,
                              CoordinateMapType>::kernel_map_type const &
CoordinateMapManager<
    coordinate_type, coordinate_field_type, TemplatedAllocator,
    CoordinateMapType>::kernel_map(CoordinateMapKey const *p_in_map_key,
                                   CoordinateMapKey const *p_out_map_key) {
  // when kernel has volume 1
  auto const &map_it = m_coordinate_maps.find(p_in_map_key->get_key());
  ASSERT(map_it != m_coordinate_maps.end(), ERROR_MAP_NOT_FOUND);
  auto const coordinate_size = map_it->second.coordinate_size();
  auto const one_vec = detail::ones(coordinate_size - 1);
  auto const offset = torch::empty(
      {0}, torch::TensorOptions().dtype(torch::kInt32).requires_grad(false));

  return kernel_map(p_in_map_key, p_out_map_key, one_vec, one_vec, one_vec,
                    RegionType::HYPER_CUBE, offset, false, false);
}

/*
 * Given tensor_stride_src and tensor_stride_dst, find the respective coord_maps
 * and return the indices of the coord_map_ind in coord_map_dst
 */
template <typename coordinate_type, typename coordinate_field_type,
          template <typename C> class TemplatedAllocator,
          template <typename T, template <typename Q> class A>
          class CoordinateMapType>
typename CoordinateMapManager<coordinate_type, coordinate_field_type,
                              TemplatedAllocator,
                              CoordinateMapType>::kernel_map_type const &
CoordinateMapManager<
    coordinate_type, coordinate_field_type, TemplatedAllocator,
    CoordinateMapType>::kernel_map(CoordinateMapKey const *p_in_map_key,
                                   CoordinateMapKey const *p_out_map_key,
                                   stride_type const &kernel_size, //
                                   stride_type const &kernel_stride,
                                   stride_type const &kernel_dilation,
                                   RegionType::Type const region_type,
                                   at::Tensor const &offset, bool is_transpose,
                                   bool is_pool) {
  ASSERT(region_type != RegionType::CUSTOM, "Not implemented yet.");
  if (region_type == RegionType::CUSTOM)
    ASSERT(offset.is_cuda() ==
               !detail::is_cpu_coordinate_map<CoordinateMapType>::value,
           "Invalid device for offset");

  size_type kernel_dim = kernel_size.size();

  ASSERT(kernel_dim == kernel_stride.size(), "kernel size mismatch");
  ASSERT(kernel_dim == kernel_dilation.size(), "kernel size mismatch");

  // in_coords_key->tensor_stride * kernel_stride ==
  // out_coords_key->tensor_stride

  kernel_map_key_type const kernel_map_key =
      std::make_tuple(p_in_map_key->get_key(), p_out_map_key->get_key(), // maps
                      kernel_size, kernel_stride, kernel_dilation, // kernels
                      region_type, is_transpose, is_pool);

  const auto &kernel_map_iter = m_kernel_maps.find(kernel_map_key);
  LOG_DEBUG("set kernel map key for kernel map:", p_in_map_key->get_key(), "->",
            p_out_map_key->get_key());

  if (kernel_map_iter == m_kernel_maps.end()) {
    // create a kernel map if it exists
    auto const in_map_it = m_coordinate_maps.find(p_in_map_key->get_key());
    auto const out_map_it = m_coordinate_maps.find(p_out_map_key->get_key());

    ASSERT(in_map_it != m_coordinate_maps.end(), "in_map", ERROR_MAP_NOT_FOUND);
    ASSERT(out_map_it != m_coordinate_maps.end(), "out_map",
           ERROR_MAP_NOT_FOUND);

    auto const &in_map = in_map_it->second;
    auto const &out_map = out_map_it->second;

    auto const D = in_map.coordinate_size();
    LOG_DEBUG("coordinate_size:", D,
              "in tensor_stride:", in_map.get_tensor_stride(),
              "out tensor_stride:", out_map.get_tensor_stride());

    // +1 for batch index
    ASSERT(kernel_dim + 1 == in_map.coordinate_size(), "kernel size mismatch");
    ASSERT(kernel_dim + 1 == out_map.coordinate_size(), "kernel size mismatch");
    if (!is_transpose) {
      if (is_pool && (kernel_stride == kernel_size)) {
        LOG_DEBUG("generating stride_map");
        auto const stride_map =
            detail::stride_map_functor<coordinate_type, TemplatedAllocator,
                                       CoordinateMapType, kernel_map_type>()(
                in_map, out_map, out_map.get_tensor_stride());

        m_kernel_maps[kernel_map_key] = std::move(stride_map);

      } else {
        LOG_DEBUG("generating kernel map");

        // Default kernel map
        auto kernel_region = cpu_kernel_region<coordinate_type>(
            region_type,                       //
            in_map.coordinate_size(),          //
            in_map.get_tensor_stride().data(), //
            kernel_size.data(),                //
            kernel_dilation.data(),            //
            0, offset.data_ptr<coordinate_type>(), offset.size(0));

        auto const kernel_map =
            detail::kernel_map_functor<coordinate_type, TemplatedAllocator,
                                       CoordinateMapType, kernel_map_type>()(
                in_map, out_map, m_kernel_map_mode, kernel_region);

        LOG_DEBUG("kernel_map done");
        m_kernel_maps[kernel_map_key] = std::move(kernel_map);
        LOG_DEBUG("kernel_map saved");
      }
    } else { // is_transpose == true
      // Check first if the out2in kernel map exists
      //
      // Create temporary key for the flipped in/out
      kernel_map_key_type const swapped_kernel_map_key = std::make_tuple(
          p_out_map_key->get_key(), p_in_map_key->get_key(), // maps
          kernel_size, kernel_stride, kernel_dilation,       // kernels
          region_type, false, is_pool);

      // Check if the temporary key exists and return swapped in/out
      if (m_kernel_maps.find(swapped_kernel_map_key) != m_kernel_maps.end()) {
        // copy the in out maps from the existing maps
        LOG_DEBUG("found existing kernel_map_key for transposed kernel map");
        m_kernel_maps[kernel_map_key] =
            detail::swap_in_out_map_functor<kernel_map_type>()(
                m_kernel_maps[swapped_kernel_map_key]);
      } else { // create in out kernel if it doesn't exist
        LOG_DEBUG("No existing kernel_map_key for transposed kernel map");
        if (is_pool && kernel_stride == kernel_size) {
          // e.g. out_map has tensor stride 2 in_map has tensor stride 4.
          // Thus, create a stride map from 2 to 4, out to in.
          auto const stride_map =
              detail::stride_map_functor<coordinate_type, TemplatedAllocator,
                                         CoordinateMapType, kernel_map_type>()(
                  out_map, in_map, kernel_stride);

          // TODO Replace the kernel_map values to shared pointers.
          m_kernel_maps[kernel_map_key] =
              detail::swap_in_out_map_functor<kernel_map_type>()(stride_map);
        } else {
          // Default kernel map
          auto kernel_region = cpu_kernel_region<coordinate_type>(
              region_type,                        //
              out_map.coordinate_size(),          //
              out_map.get_tensor_stride().data(), //
              kernel_size.data(),                 //
              kernel_dilation.data(),             //
              0, offset.data_ptr<coordinate_type>(), offset.size(0),
              true // is_transpose
          );

          // out to in kernel map
          auto const kernel_map =
              detail::kernel_map_functor<coordinate_type, TemplatedAllocator,
                                         CoordinateMapType, kernel_map_type>()(
                  out_map, in_map, m_kernel_map_mode, kernel_region);

          LOG_DEBUG("kernel_map done");
          m_kernel_maps[kernel_map_key] =
              detail::swap_in_out_map_functor<kernel_map_type>()(
                  std::move(kernel_map));
          LOG_DEBUG("kernel_map saved");
        }
      }
    }
  }
#ifdef DEBUG
  else {
    LOG_DEBUG("kernel map found");
  }
#endif

  // TODO check if it copies or moves the internal data
  return m_kernel_maps[kernel_map_key];
}

namespace detail {

template <typename coordinate_type>
struct origin_map_functor<coordinate_type, std::allocator, CoordinateMapCPU,
                          cpu_kernel_map> {

  std::pair<at::Tensor, std::vector<at::Tensor>>
  operator()(CoordinateMapCPU<coordinate_type, std::allocator> const
                 &origin_coordinate_map,
             cpu_kernel_map const &origin_map) {

    auto options =
        torch::TensorOptions().dtype(torch::kLong).requires_grad(false);
    auto const out_size = origin_coordinate_map.size();
    auto const coordinate_size = origin_coordinate_map.coordinate_size();

    at::Tensor batch_indices =
        torch::empty({origin_coordinate_map.size()}, options);
    int64_t *p_batch_indices = batch_indices.data_ptr<int64_t>();

    LOG_DEBUG("Copying", origin_coordinate_map.size(), "batch indices");
    for (default_types::index_type i = 0; i < out_size; ++i) {
      p_batch_indices[i] =
          origin_coordinate_map.const_coordinate_data()[i * coordinate_size];
    }

    // WARNING: this is an inclusive max index
    coordinate_type const max_batch_index =
        *std::max_element(p_batch_indices, p_batch_indices + out_size);

    std::vector<at::Tensor> in_maps;
    for (uint32_t i = 0; i <= max_batch_index; ++i) {
      at::Tensor row_indices = torch::empty({0}, options);
      in_maps.push_back(std::move(row_indices));
    }

    ASSERT(origin_map.first.size() == origin_map.second.size(),
           "invalid kernel_map");
    LOG_DEBUG("Iterating over", origin_map.first.size(), "unique maps");
    for (uint32_t out_row_index = 0; out_row_index < origin_map.first.size();
         ++out_row_index) {
      auto const &in_map = origin_map.first[out_row_index];
      int32_t const curr_size = in_map.size();
      ASSERT(curr_size > 0, "invalid kernel map");
      auto const curr_batch_index = p_batch_indices[out_row_index];

      ASSERT(curr_batch_index <= max_batch_index, "invalid batch index");
      at::Tensor &row_indices = in_maps[curr_batch_index];
      row_indices.resize_({curr_size});
      int64_t *p_row_indices = row_indices.data_ptr<int64_t>();

      LOG_DEBUG("Copying", curr_size, "elements to batch index",
                curr_batch_index, "and row index", out_row_index);
      for (default_types::index_type i = 0; i < curr_size; ++i) {
        p_row_indices[i] = in_map[i];
      }
    }

    return std::make_pair(batch_indices, in_maps);
  }
};

} // namespace detail

template <typename coordinate_type, typename coordinate_field_type,
          template <typename C> class TemplatedAllocator,
          template <typename T, template <typename Q> class A>
          class CoordinateMapType>
typename CoordinateMapManager<coordinate_type, coordinate_field_type,
                              TemplatedAllocator,
                              CoordinateMapType>::kernel_map_type const &
CoordinateMapManager<coordinate_type, coordinate_field_type, TemplatedAllocator,
                     CoordinateMapType>::origin_map(CoordinateMapKey const
                                                        *p_in_map_key) {
  ASSERT(exists(p_in_map_key), ERROR_MAP_NOT_FOUND);
  kernel_map_key_type const kernel_map_key =
      origin_map_key(p_in_map_key->get_key());
  coordinate_map_key_type const origin_key = std::get<1>(kernel_map_key);

  if (m_kernel_maps.find(kernel_map_key) == m_kernel_maps.end()) {
    auto const key = origin().first;
    auto const &origin_coordinate_map = m_coordinate_maps.find(key)->second;
    auto origin_map = m_coordinate_maps.find(p_in_map_key->get_key())
                          ->second.origin_map(origin_coordinate_map);
    m_kernel_maps[kernel_map_key] = std::move(origin_map);
  }

  return m_kernel_maps[kernel_map_key];
}

template <typename coordinate_type, typename coordinate_field_type,
          template <typename C> class TemplatedAllocator,
          template <typename T, template <typename Q> class A>
          class CoordinateMapType>
std::pair<at::Tensor, std::vector<at::Tensor>>
CoordinateMapManager<coordinate_type, coordinate_field_type, TemplatedAllocator,
                     CoordinateMapType>::origin_map_th(CoordinateMapKey const
                                                           *p_in_map_key) {
  kernel_map_type const &kernel_map = origin_map(p_in_map_key);

  coordinate_map_key_type const origin_key = origin().first;
  map_type const &origin_map = m_coordinate_maps.find(origin_key)->second;

  return detail::origin_map_functor<coordinate_type, TemplatedAllocator,
                                    CoordinateMapType, kernel_map_type>()(
      origin_map, kernel_map);
}

/*********************************/
/*
template <typename MapType>
uint64_t
CoordsManager<MapType>::createUnionCoords(vector<py::object> py_in_coords_keys,
                                          py::object py_out_coords_key) {
  vector<CoordsKey *> p_in_coords_keys;
  CoordsKey *p_in_coords_key = py_in_coords_keys[0].cast<CoordsKey *>();
  auto tensor_strides = p_in_coords_key->getTensorStride();
  for (const auto &py_in_coords_key : py_in_coords_keys) {
    // Set the tensor strides to the smallest elements.
    p_in_coords_key = py_in_coords_key.cast<CoordsKey *>();
    p_in_coords_keys.push_back(p_in_coords_key);
    transform(tensor_strides.begin(),                            // In1 begin
              tensor_strides.end(),                              // In1 end
              p_in_coords_key->getTensorStride().begin(),        // In2 begin
              tensor_strides.begin(),                            // out begin
              [](int a, int b) -> int { return std::min(a, b); } // binary op
    );
    const uint64_t in_coords_key = p_in_coords_key->getKey();
    ASSERT(existsCoordsKey(in_coords_key),
           "The coord map doesn't exist for the given coords_key: ",
           to_string(in_coords_key), ".");
  }
  CoordsKey *p_out_coords_key = py_out_coords_key.cast<CoordsKey *>();

  vector<reference_wrapper<CoordsMap<MapType>>> in_coords_maps;
  for (const CoordsKey *p_in_coords_key : p_in_coords_keys) {
    CoordsMap<MapType> &curr_map = coords_maps[p_in_coords_key->getKey()];
    in_coords_maps.push_back(ref(curr_map));
  }

  // set a random coords key
  const uint64_t out_coords_key = getRandomCoordsKey();

  // Set the pycoordskey using the last coords_key
  p_out_coords_key->setDimension(p_in_coords_key->getDimension());
  p_out_coords_key->setKey(out_coords_key);
  p_out_coords_key->setTensorStride(tensor_strides);

  coords_maps[out_coords_key] =
      CoordsMap<MapType>::union_coords(in_coords_maps);

  return out_coords_key;
}

template <typename MapType>
const InOutMapKey
CoordsManager<MapType>::getUnionMapHashKey(vector<py::object> py_in_coords_keys,
                                           py::object py_out_coords_key) const {
  CoordsKey *p_out_coords_key = py_out_coords_key.cast<CoordsKey *>();
  ASSERT(py_in_coords_keys.size() > 1, "Number of input coords must be > 1");
  vector<CoordsKey *> p_in_coords_keys;
  // We use sum of coords key (even with overflow, it will be unique with high
  // prob). We use sum to make the key invariant to the order of the keys.
  uint64_t sum_in_coords_key = 0;
  CoordsKey *p_in_coords_key = py_in_coords_keys[0].cast<CoordsKey *>();
  for (auto &py_in_coords_key : py_in_coords_keys) {
    p_in_coords_key = py_in_coords_key.cast<CoordsKey *>();
    const uint64_t in_coords_key = p_in_coords_key->getKey();
    ASSERT(existsCoordsKey(in_coords_key),
           "The coord map doesn't exist for the given coords_key: ",
           to_string(in_coords_key), ".");
    sum_in_coords_key += in_coords_key;
  }

  ASSERT(p_out_coords_key->key_set, "Key is not set. out_coords_key: ",
         to_string(p_out_coords_key->getKey()));

  const uint64_t out_coords_key = p_out_coords_key->getKey();
  const vector<int> zero_vec(p_in_coords_key->getDimension(), 0);
  const uint64_t zero_hash = hash_vec(zero_vec);
  InOutMapKey map_key = {sum_in_coords_key,
                         out_coords_key,
                         zero_hash,
                         zero_hash,
                         zero_hash,
                         0,
                         false,
                         true};
  return map_key;
}
*/
/**
 * Entry function for coords map generation and the associated kernel maps.
 */
/*
template <typename MapType>
const InOutMapsRefPair<int>
CoordsManager<MapType>::getPruningInOutMaps(at::Tensor use_feat,
                                            py::object py_in_coords_key,
                                            py::object py_out_coords_key) {
  CoordsKey *p_in_coords_key = py_in_coords_key.cast<CoordsKey *>();
  CoordsKey *p_out_coords_key = py_out_coords_key.cast<CoordsKey *>();

  // Create output coordinates if it doesn't exist
  if (!p_out_coords_key->key_set) {
    // The following function setup py_out_coords_key
    createPrunedCoords(use_feat, py_in_coords_key, py_out_coords_key);
  }

  const uint64_t in_coords_key = p_in_coords_key->getKey();
  const uint64_t out_coords_key = p_out_coords_key->getKey();

  // Use the map key for origin hash map (stride, dilation, kernel are all
  // NULL)
  const InOutMapKey map_key =
      getOriginMapHashKey(py_in_coords_key, py_out_coords_key);

  // For non transpose case
  // make a kernel mapping. The kernel will be saved with the map_key.
  if (in_maps.find(map_key) == in_maps.end()) {
    const auto in_out = coords_maps[in_coords_key].pruned_kernel_map(
        coords_maps[out_coords_key]);
    in_maps[map_key] = in_out.first;
    out_maps[map_key] = in_out.second;
  }

  return make_pair(ref(in_maps[map_key]), ref(out_maps[map_key]));
}

template <typename MapType>
const InOutMapsRefPair<int>
CoordsManager<MapType>::getUnionInOutMaps(vector<py::object> py_in_coords_keys,
                                          py::object py_out_coords_key) {
  CoordsKey *p_out_coords_key = py_out_coords_key.cast<CoordsKey *>();

  // Create output coordinates if it doesn't exist
  if (!p_out_coords_key->key_set)
    createUnionCoords(py_in_coords_keys, py_out_coords_key);

  const uint64_t out_coords_key = p_out_coords_key->getKey();

  // Map key for origin hash map
  const InOutMapKey map_key =
      getUnionMapHashKey(py_in_coords_keys, py_out_coords_key);

  vector<reference_wrapper<CoordsMap<MapType>>> in_coords_maps;
  for (const auto &py_in_coords_key : py_in_coords_keys) {
    const CoordsKey *p_in_coords_key = py_in_coords_key.cast<CoordsKey *>();
    uint64_t in_coords_key = p_in_coords_key->getKey();
    in_coords_maps.push_back(ref(coords_maps[in_coords_key]));
  }

  // For non transpose case
  // make a kernel mapping. The kernel will be saved with the map_key.
  if (in_maps.find(map_key) == in_maps.end()) {
    const auto in_out = CoordsMap<MapType>::union_map(
        in_coords_maps, coords_maps[out_coords_key]);
    in_maps[map_key] = in_out.first;
    out_maps[map_key] = in_out.second;
  }

  return make_pair(ref(in_maps[map_key]), ref(out_maps[map_key]));
}
*/

/* Helper functions */
template <typename coordinate_type, typename coordinate_field_type,
          template <typename C> class TemplatedAllocator,
          template <typename T, template <typename Q> class A>
          class CoordinateMapType>
at::Tensor
CoordinateMapManager<coordinate_type, coordinate_field_type, TemplatedAllocator,
                     CoordinateMapType>::get_coordinates(CoordinateMapKey const
                                                             *p_key) const {
  ASSERT(exists(p_key), ERROR_MAP_NOT_FOUND);
  auto const it = m_coordinate_maps.find(p_key->get_key());
  ASSERT(it != m_coordinate_maps.end(), ERROR_MAP_NOT_FOUND);
  auto const &map = it->second;
  auto const nrows = map.size();
  auto const ncols = map.coordinate_size();

  // CPU torch.IntTensor
  auto options = torch::TensorOptions().dtype(torch::kInt).requires_grad(false);
  if (!detail::is_cpu_coordinate_map<CoordinateMapType>::value) {
#ifndef CPU_ONLY
    auto device_id = at::cuda::current_device();
    options = options.device(torch::kCUDA, device_id);
#else
    ASSERT(false, ERROR_CPU_ONLY);
#endif
  }
  at::Tensor coordinates = torch::empty({(long)nrows, (long)ncols}, options);

  // copy to the out coords
  map.copy_coordinates(coordinates.template data_ptr<coordinate_type>());
  return coordinates;
}

template <typename coordinate_type, typename coordinate_field_type,
          template <typename C> class TemplatedAllocator,
          template <typename T, template <typename Q> class A>
          class CoordinateMapType>
at::Tensor CoordinateMapManager<coordinate_type, coordinate_field_type,
                                TemplatedAllocator, CoordinateMapType>::
    get_coordinate_field(CoordinateMapKey const *p_key) const {
  ASSERT(exists(p_key), ERROR_MAP_NOT_FOUND);
  auto const it = m_field_coordinates.find(p_key->get_key());
  ASSERT(it != m_field_coordinates.end(), ERROR_MAP_NOT_FOUND);
  auto const &map = it->second;
  auto const nrows = map.size();
  auto const ncols = map.coordinate_size();

  auto options = torch::TensorOptions()
                     .dtype(std::is_same<float, coordinate_field_type>::value
                                ? torch::kFloat
                                : torch::kDouble)
                     .requires_grad(false);

  if (!detail::is_cpu_coordinate_map<CoordinateMapType>::value) {
#ifndef CPU_ONLY
    auto device_id = at::cuda::current_device();
    options = options.device(torch::kCUDA, device_id);
#else
    ASSERT(false, ERROR_CPU_ONLY);
#endif
  }
  at::Tensor coordinates = torch::empty({(long)nrows, (long)ncols}, options);

  // copy to the out coords
  map.copy_coordinates(coordinates.template data_ptr<coordinate_field_type>());
  return coordinates;
}

template class CoordinateMapManager<default_types::dcoordinate_type,
                                    default_types::ccoordinate_type,
                                    std::allocator, CoordinateMapCPU>;

} // end namespace minkowski
