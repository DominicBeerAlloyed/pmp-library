// Copyright 2011-2020 the Polygon Mesh Processing Library developers.
// Distributed under a MIT-style license, see LICENSE.txt for details.

#include "pmp/algorithms/fairing.h"
#include "pmp/algorithms/laplace.h"

namespace pmp {

void minimize_area(SurfaceMesh& mesh)
{
    fair(mesh, 1);
}

void minimize_curvature(SurfaceMesh& mesh)
{
    fair(mesh, 2);
}

void fair(SurfaceMesh& mesh, unsigned int k)
{
    // get & add properties
    auto points = mesh.vertex_property<Point>("v:point");
    auto vselected = mesh.get_vertex_property<bool>("v:selected");
    auto vlocked = mesh.add_vertex_property<bool>("fairing:locked");

    auto cleanup = [&]() { mesh.remove_vertex_property(vlocked); };

    // check whether some vertices are selected
    bool no_selection = true;
    if (vselected)
    {
        for (auto v : mesh.vertices())
        {
            if (vselected[v])
            {
                no_selection = false;
                break;
            }
        }
    }

    // lock k locked boundary rings
    for (auto v : mesh.vertices())
    {
        // lock boundary
        if (mesh.is_boundary(v))
        {
            vlocked[v] = true;

            // lock one-ring of boundary
            if (k > 1)
            {
                for (auto vv : mesh.vertices(v))
                {
                    vlocked[vv] = true;

                    // lock two-ring of boundary
                    if (k > 2)
                    {
                        for (auto vvv : mesh.vertices(vv))
                        {
                            vlocked[vvv] = true;
                        }
                    }
                }
            }
        }
    }

    // lock un-selected and isolated vertices
    for (auto v : mesh.vertices())
    {
        if (!no_selection && !vselected[v])
        {
            vlocked[v] = true;
        }

        if (mesh.is_isolated(v))
        {
            vlocked[v] = true;
        }
    }

    // we need locked vertices as boundary constraints
    bool something_locked = false;
    for (auto v : mesh.vertices())
    {
        if (vlocked[v])
        {
            something_locked = true;
            break;
        }
    }
    if (!something_locked)
    {
        cleanup();
        auto what = std::string{__func__} + ": Missing boundary constraints.";
        throw InvalidInputException(what);
    }

    const int n = mesh.n_vertices();

    // build (zero) right-hand side B
    DenseMatrix B(n, 3);
    B.setZero();

    // positions will be used as constraints
    DenseMatrix X(n, 3);
    for (auto v : mesh.vertices())
        X.row(v.idx()) = static_cast<Eigen::Vector3d>(points[v]);

    // build matrix
    SparseMatrix S;
    setup_stiffness_matrix(mesh, S, false, true);
    DiagonalMatrix M;
    setup_mass_matrix(mesh, M, false);
    DiagonalMatrix invM = M.inverse();
    SparseMatrix A = S;
    for (unsigned int i = 1; i < k; ++i)
        A = S * invM * A;
    B = M * B;

    // solve system
    auto is_locked = [&](unsigned int i) { return vlocked[Vertex(i)]; };
    X = cholesky_solve(A, B, is_locked, X);

    // copy solution
    for (auto v : mesh.vertices())
        points[v] = X.row(v.idx());

    // remove properties
    cleanup();
}

} // namespace pmp
