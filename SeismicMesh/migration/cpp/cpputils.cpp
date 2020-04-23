#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/complex.h>
#include <pybind11/numpy.h>

#include <CGAL/Kernel/global_functions.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Iso_rectangle_2.h>
#include <CGAL/Circle_2.h>
#include <CGAL/intersections.h>

#include <assert.h>
#include <vector>

typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef K::Point_2 Point;
typedef K::Circle_2  Circle;
typedef K::Iso_rectangle_2 Rectangle;

namespace py = pybind11;


// determine which rank points need to be exported to
std::vector<double> c_where_to2(std::vector<double> &points, std::vector<int> &faces,
                             std::vector<int> &vtoe, std::vector<int> &ptr,
                             std::vector<double> &llc, std::vector<double> &urc, int rank)
{
    int num_faces = faces.size()/3;
    int num_points = points.size()/2;

    // Determine which rank to send the vertex (exports)
    // exports[iv] is either 0 or 1 (0 for block owned by rank-1 and 1 for block owned by rank+1)
    std::vector<int> exports;
    exports.resize(num_points,-1);
    // For each point in points
    for(std::size_t iv=0; iv < num_points; ++iv)
    {
        int nneis = ptr[iv+1]-ptr[iv] + 1;
        // For all connected elements to point iv
        for(std::size_t ic=0; ic < nneis; ++ic)
        {
            int nei_ele = vtoe[ptr[iv]+ic];
            // Indices of element into points
            int nm1 = faces[nei_ele*3];
            int nm2 = faces[nei_ele*3+1];
            int nm3 = faces[nei_ele*3+2];
            // Coordinates of each vertex of element
            Point pnm1 = Point(points[nm1*2], points[nm1*2+1]);
            Point pnm2 = Point(points[nm2*2], points[nm2*2+1]);
            Point pnm3 = Point(points[nm3*2], points[nm3*2+1]);
            // Calculate circumball of element
            Point cc = CGAL::circumcenter(pnm1, pnm2, pnm3);
            double sqr_radius = CGAL::squared_radius(pnm1, pnm2, pnm3);
            Circle circ = Circle(cc, sqr_radius, CGAL::CLOCKWISE);
            // Does this circumball intersect with box above or box below?
            for(std::size_t bx=0; bx< 2; ++bx ){
                Rectangle rect = Rectangle(Point(llc[bx*2], llc[bx*2 +1]),
                        Point(urc[bx*2], urc[bx*2+1]));
                bool intersects = CGAL::do_intersect(circ, rect);
                if(intersects){
                    exports[iv] = bx;
                }
            }
        }
    }

    std::vector<double> pointsToMigrate;
    pointsToMigrate.resize(num_points*3,-1);

    double kount_below = 0.0;
    for(std::size_t iv=0; iv < num_points; ++iv)
    {
        if(exports[iv]==0)
        {
            pointsToMigrate[kount_below*3+0+3]=points[iv*2];
            pointsToMigrate[kount_below*3+1+3]=points[iv*2+1];
            pointsToMigrate[kount_below*3+2+3]=(double)iv;
            kount_below += 1.0;
        }
    }

    double kount_above = 0.0;
    for(std::size_t iv=0; iv < num_points; ++iv)
    {
        if(exports[iv]==1)
        {
            pointsToMigrate[kount_below*3 + kount_above*3+0+3]=points[iv*2];
            pointsToMigrate[kount_below*3 + kount_above*3+1+3]=points[iv*2+1];
            pointsToMigrate[kount_below*3 + kount_above*3+2+3]=(double)iv;
            kount_above += 1.0;
        }
    }
    pointsToMigrate[0] = kount_below;
    pointsToMigrate[1] = kount_above;
    pointsToMigrate[2] = 0.0;

    return pointsToMigrate;
}

// ----------------
// Python interface for c_where_to2
// ----------------
py::array where_to2(py::array_t<double, py::array::c_style | py::array::forcecast> points,
                    py::array_t<int, py::array::c_style | py::array::forcecast> faces,
                    py::array_t<int, py::array::c_style | py::array::forcecast> vtoe,
                    py::array_t<int, py::array::c_style | py::array::forcecast> ptr,
                    py::array_t<double, py::array::c_style | py::array::forcecast> llc,
                    py::array_t<double, py::array::c_style | py::array::forcecast> urc,
                    int rank
                    )
{
  int num_faces = faces.size()/3;
  int num_points = points.size()/2;

  // allocate std::vector (to pass to the C++ function)
  std::vector<double> cpppoints(num_points*2);
  std::vector<int> cppfaces(num_faces*3);
  std::vector<int> cppvtoe(num_faces*3);
  std::vector<int> cppptr(num_points+1);
  std::vector<double> cppllc(4);
  std::vector<double> cppurc(4);

  // copy py::array -> std::vector
  std::memcpy(cpppoints.data(),points.data(),num_points*2*sizeof(double));
  std::memcpy(cppfaces.data(),faces.data(),num_faces*3*sizeof(int));
  std::memcpy(cppvtoe.data(),vtoe.data(),num_faces*3*sizeof(int));
  std::memcpy(cppptr.data(),ptr.data(),(num_points+1)*sizeof(int));
  std::memcpy(cppllc.data(), llc.data(),4*sizeof(double));
  std::memcpy(cppurc.data(), urc.data(),4*sizeof(double));

  // call cpp code
  std::vector<double> pointsToMigrate = c_where_to2(cpppoints, cppfaces, cppvtoe, cppptr, cppllc, cppurc, rank);

  ssize_t              sodble    = sizeof(double);
  ssize_t              ndim      = 2;
  std::vector<ssize_t> shape     = {num_points+1, 3};
  std::vector<ssize_t> strides   = {sodble*3, sodble};

  // return 2-D NumPy array
  return py::array(py::buffer_info(
    pointsToMigrate.data(),                /* data as contiguous array  */
    sizeof(double),                       /* size of one scalar        */
    py::format_descriptor<double>::format(), /* data type                 */
    2,                                    /* number of dimensions      */
    shape,                                   /* shape of the matrix       */
    strides                                  /* strides for each axis     */
  ));
}


PYBIND11_MODULE(cpputils, m) {
    m.def("where_to2", &where_to2);
}
