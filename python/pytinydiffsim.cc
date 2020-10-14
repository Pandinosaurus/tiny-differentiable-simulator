// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "math/tiny/tiny_double_utils.h"


typedef double TinyDualScalar;
typedef double MyScalar;
typedef ::TINY::DoubleUtils MyTinyConstants;

#include "math/tiny/tiny_algebra.hpp"
#include "dynamics/mass_matrix.hpp"
#include "dynamics/kinematics.hpp"
#include "dynamics/forward_dynamics.hpp"
#include "dynamics/integrator.hpp"

typedef TinyAlgebra<double, MyTinyConstants> MyAlgebra;

#include "pytinydiffsim_includes.h"

using namespace TINY;
using namespace tds;

namespace py = pybind11;
MyScalar fraction(int a, int b)
{
return MyTinyConstants::fraction(a,b);
}



void MyMassMatrix(MultiBody<MyAlgebra>& mb, MyAlgebra::VectorX& q,
    MyAlgebra::MatrixX* M)
{
    mass_matrix(mb, q, M);
}

void MyForwardKinematics(MultiBody<MyAlgebra>& mb, const MyAlgebra::VectorX& q)
{
    forward_kinematics(mb, q);
}

void MyForwardDynamics(MultiBody<MyAlgebra>& mb, const MyAlgebra::Vector3& gravity)
{
    forward_dynamics(mb, gravity);
}
void MyIntegrateEuler(MultiBody<MyAlgebra>& mb, const MyScalar& dt)
{
    integrate_euler(mb, dt);
}


PYBIND11_MODULE(pytinydiffsim2, m) {


#include "pytinydiffsim.inl"

}