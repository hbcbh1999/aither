/*  This file is part of aither.
    Copyright (C) 2015  Michael Nucci (michael.nucci@gmail.com)

    Aither is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Aither is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <utility>  // pair
#include <cmath>
#include "output.hpp"
#include "turbulence.hpp"
#include "vector3d.hpp"  // vector3d
#include "multiArray3d.hpp"  // multiArray3d
#include "tensor.hpp"    // tensor
#include "plot3d.hpp"    // plot3d
#include "eos.hpp"
#include "primVars.hpp"            // primVars
#include "procBlock.hpp"           // procBlock
#include "inviscidFlux.hpp"        // inviscidFlux
#include "input.hpp"               // inputVars
#include "boundaryConditions.hpp"  // decomposition
#include "resid.hpp"               // resid
#include "genArray.hpp"            // genArray

using std::cout;
using std::endl;
using std::cerr;
using std::vector;
using std::string;
using std::ios;
using std::ofstream;
using std::to_string;
using std::max;
using std::pair;
using std::setw;
using std::setprecision;
using std::unique_ptr;

//-----------------------------------------------------------------------
// function declarations
// function to write out cell centers of grid in plot3d format
void WriteCellCenter(const string &gridName, const vector<procBlock> &vars,
                     const decomposition &decomp, const double &LRef) {
  // recombine procblocks into original configuration
  auto recombVars = Recombine(vars, decomp);

  // open binary output file
  const string fEnd = "_center";
  const string fPostfix = ".xyz";
  const auto writeName = gridName + fEnd + fPostfix;
  ofstream outFile(writeName, ios::out | ios::binary);

  // check to see if file opened correctly
  if (outFile.fail()) {
    cerr << "ERROR: Grid file " << writeName << " did not open correctly!!!"
         << endl;
    exit(EXIT_FAILURE);
  }

  // write number of blocks to file
  auto numBlks = static_cast<int>(recombVars.size());
  outFile.write(reinterpret_cast<char *>(&numBlks), sizeof(numBlks));

  // write i, j, k dimension for each block
  for (auto ll = 0; ll < numBlks; ll++) {  // loop over all blocks
    auto dumInt = recombVars[ll].NumI();
    outFile.write(reinterpret_cast<char *>(&dumInt), sizeof(dumInt));
    dumInt = recombVars[ll].NumJ();
    outFile.write(reinterpret_cast<char *>(&dumInt), sizeof(dumInt));
    dumInt = recombVars[ll].NumK();
    outFile.write(reinterpret_cast<char *>(&dumInt), sizeof(dumInt));
  }

  // write out x, y, z coordinates of cell centers
  for (auto ll = 0; ll < numBlks; ll++) {  // loop over all blocks
    auto maxi = recombVars[ll].NumI();
    auto maxj = recombVars[ll].NumJ();
    auto maxk = recombVars[ll].NumK();

    for (auto nn = 0; nn < 3; nn++) {  // loop over dimensions (3)
      for (auto kk = recombVars[ll].NumGhosts();
           kk < maxk + recombVars[ll].NumGhosts(); kk++) {
        for (auto jj = recombVars[ll].NumGhosts();
             jj < maxj + recombVars[ll].NumGhosts(); jj++) {
          for (auto ii = recombVars[ll].NumGhosts();
               ii < maxi + recombVars[ll].NumGhosts(); ii++) {
            // get the cell center coordinates (dimensionalized)
            auto dumVec = recombVars[ll].Center(ii, jj, kk) * LRef;

            // for a given block, first write out all x coordinates, then all y
            // coordinates, then all z coordinates
            auto dumDouble = dumVec[nn];
            // write to file
            outFile.write(reinterpret_cast<char *>(&dumDouble),
                          sizeof(dumDouble));
          }
        }
      }
    }
  }

  // close output file
  outFile.close();
}

//----------------------------------------------------------------------
// function to write out variables in function file format
void WriteFun(const vector<procBlock> &vars, const idealGas &eqnState,
              const sutherland &suth, const int &solIter,
              const decomposition &decomp, const input &inp,
              const unique_ptr<turbModel> &turb) {
  // define reference speed of sound
  const auto refSoS = eqnState.SoS(inp.PRef(), inp.RRef());

  auto recombVars = Recombine(vars, decomp);

  // open binary plot3d function file
  const string fEnd = "_center";
  const string fPostfix = ".fun";
  const auto writeName = inp.SimNameRoot() + "_" + to_string(solIter) + fEnd +
      fPostfix;
  ofstream outFile(writeName, ios::out | ios::binary);

  // check to see if file opened correctly
  if (outFile.fail()) {
    cerr << "ERROR: Function file " << writeName << " did not open correctly!!!"
         << endl;
    exit(EXIT_FAILURE);
  }

  // write number of blocks to file
  auto numBlks = static_cast<int>(recombVars.size());
  outFile.write(reinterpret_cast<char *>(&numBlks), sizeof(numBlks));

  // write i, j, k, recombVars dimension for each block
  auto numVars = inp.NumVarsOutput();  // number of variables to write out

  // loop over all blocks and write out imax, jmax, kmax, numVars
  for (auto ll = 0; ll < numBlks; ll++) {
    auto dumInt = recombVars[ll].NumI();
    outFile.write(reinterpret_cast<char *>(&dumInt), sizeof(dumInt));
    dumInt = recombVars[ll].NumJ();
    outFile.write(reinterpret_cast<char *>(&dumInt), sizeof(dumInt));
    dumInt = recombVars[ll].NumK();
    outFile.write(reinterpret_cast<char *>(&dumInt), sizeof(dumInt));

    outFile.write(reinterpret_cast<char *>(&numVars), sizeof(numVars));
  }

  // write out variables
  for (auto ll = 0; ll < numBlks; ll++) {  // loop over all blocks
    multiArray3d<double> dumArr(recombVars[ll].NumI(), recombVars[ll].NumJ(),
                                recombVars[ll].NumK());

    // loop over the number of variables to write out
    for (auto &var : inp.OutputVariables()) {
      // store nondimensional variable in dumArr for a given block in order.
      // i.e. var1 var2 var3 etc
      if (var == "density") {
        for (auto kp = 0, kg = recombVars[ll].NumGhosts();
             kp < recombVars[ll].NumK(); kg++, kp++) {
          for (auto jp = 0, jg = recombVars[ll].NumGhosts();
               jp < recombVars[ll].NumJ(); jg++, jp++) {
            for (auto ip = 0, ig = recombVars[ll].NumGhosts();
                 ip < recombVars[ll].NumI(); ig++, ip++) {
              dumArr(ip, jp, kp) =
                  recombVars[ll].State(ig, jg, kg).Rho();
            }
          }
        }
      } else if (var == "vel_x") {
        for (auto kp = 0, kg = recombVars[ll].NumGhosts();
             kp < recombVars[ll].NumK(); kg++, kp++) {
          for (auto jp = 0, jg = recombVars[ll].NumGhosts();
               jp < recombVars[ll].NumJ(); jg++, jp++) {
            for (auto ip = 0, ig = recombVars[ll].NumGhosts();
                 ip < recombVars[ll].NumI(); ig++, ip++) {
              dumArr(ip, jp, kp) =
                  recombVars[ll].State(ig, jg, kg).U();
            }
          }
        }
      } else if (var == "vel_y") {
        for (auto kp = 0, kg = recombVars[ll].NumGhosts();
             kp < recombVars[ll].NumK(); kg++, kp++) {
          for (auto jp = 0, jg = recombVars[ll].NumGhosts();
               jp < recombVars[ll].NumJ(); jg++, jp++) {
            for (auto ip = 0, ig = recombVars[ll].NumGhosts();
                 ip < recombVars[ll].NumI(); ig++, ip++) {
              dumArr(ip, jp, kp) =
                  recombVars[ll].State(ig, jg, kg).V();
            }
          }
        }
      } else if (var == "vel_z") {
        for (auto kp = 0, kg = recombVars[ll].NumGhosts();
             kp < recombVars[ll].NumK(); kg++, kp++) {
          for (auto jp = 0, jg = recombVars[ll].NumGhosts();
               jp < recombVars[ll].NumJ(); jg++, jp++) {
            for (auto ip = 0, ig = recombVars[ll].NumGhosts();
                 ip < recombVars[ll].NumI(); ig++, ip++) {
              dumArr(ip, jp, kp) =
                  recombVars[ll].State(ig, jg, kg).W();
            }
          }
        }
      } else if (var == "pressure") {
        for (auto kp = 0, kg = recombVars[ll].NumGhosts();
             kp < recombVars[ll].NumK(); kg++, kp++) {
          for (auto jp = 0, jg = recombVars[ll].NumGhosts();
               jp < recombVars[ll].NumJ(); jg++, jp++) {
            for (auto ip = 0, ig = recombVars[ll].NumGhosts();
                 ip < recombVars[ll].NumI(); ig++, ip++) {
              dumArr(ip, jp, kp) =
                  recombVars[ll].State(ig, jg, kg).P();
            }
          }
        }
      } else if (var == "mach") {
        for (auto kp = 0, kg = recombVars[ll].NumGhosts();
             kp < recombVars[ll].NumK(); kg++, kp++) {
          for (auto jp = 0, jg = recombVars[ll].NumGhosts();
               jp < recombVars[ll].NumJ(); jg++, jp++) {
            for (auto ip = 0, ig = recombVars[ll].NumGhosts();
                 ip < recombVars[ll].NumI(); ig++, ip++) {
              const auto vel =
                  recombVars[ll].State(ig, jg, kg).Velocity();
              dumArr(ip, jp, kp) = vel.Mag() /
                  eqnState.SoS(recombVars[ll].State(ig, jg, kg).P(),
                               recombVars[ll].State(ig, jg, kg).Rho());
            }
          }
        }
      } else if (var == "sos") {
        for (auto kp = 0, kg = recombVars[ll].NumGhosts();
             kp < recombVars[ll].NumK(); kg++, kp++) {
          for (auto jp = 0, jg = recombVars[ll].NumGhosts();
               jp < recombVars[ll].NumJ(); jg++, jp++) {
            for (auto ip = 0, ig = recombVars[ll].NumGhosts();
                 ip < recombVars[ll].NumI(); ig++, ip++) {
              dumArr(ip, jp, kp) =
                  eqnState.SoS(recombVars[ll].State(ig, jg, kg).P(),
                               recombVars[ll].State(ig, jg, kg).Rho());
            }
          }
        }
      } else if (var == "dt") {  // time step - no ghost cells
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].Dt(ii, jj, kk);
            }
          }
        }
      } else if (var == "temperature") {
        for (auto kp = 0, kg = recombVars[ll].NumGhosts();
             kp < recombVars[ll].NumK(); kg++, kp++) {
          for (auto jp = 0, jg = recombVars[ll].NumGhosts();
               jp < recombVars[ll].NumJ(); jg++, jp++) {
            for (auto ip = 0, ig = recombVars[ll].NumGhosts();
                 ip < recombVars[ll].NumI(); ig++, ip++) {
              dumArr(ip, jp, kp) = recombVars[ll].Temperature(ig, jg, kg);
            }
          }
        }
      } else if (var == "rank") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = vars[SplitBlockNumber(recombVars, decomp, ll,
                                                         ii, jj, kk)].Rank();
            }
          }
        }
      } else if (var == "globalPosition") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) =
                  vars[SplitBlockNumber(recombVars, decomp, ll,
                                        ii, jj, kk)].GlobalPos();
            }
          }
        }
      } else if (var == "viscosityRatio") {
        for (auto kp = 0, kg = recombVars[ll].NumGhosts();
             kp < recombVars[ll].NumK(); kg++, kp++) {
          for (auto jp = 0, jg = recombVars[ll].NumGhosts();
               jp < recombVars[ll].NumJ(); jg++, jp++) {
            for (auto ip = 0, ig = recombVars[ll].NumGhosts();
                 ip < recombVars[ll].NumI(); ig++, ip++) {
              dumArr(ip, jp, kp) = recombVars[ll].IsTurbulent() ?
                  recombVars[ll].EddyViscosity(ig, jg, kg) /
                  recombVars[ll].Viscosity(ig, jg, kg)
                  : 0.0;
            }
          }
        }
      } else if (var == "tke") {
        for (auto kp = 0, kg = recombVars[ll].NumGhosts();
             kp < recombVars[ll].NumK(); kg++, kp++) {
          for (auto jp = 0, jg = recombVars[ll].NumGhosts();
               jp < recombVars[ll].NumJ(); jg++, jp++) {
            for (auto ip = 0, ig = recombVars[ll].NumGhosts();
                 ip < recombVars[ll].NumI(); ig++, ip++) {
              dumArr(ip, jp, kp) =
                  recombVars[ll].State(ig, jg, kg).Tke();
            }
          }
        }
      } else if (var == "sdr") {  // omega (specific dissipation rate)
        for (auto kp = 0, kg = recombVars[ll].NumGhosts();
             kp < recombVars[ll].NumK(); kg++, kp++) {
          for (auto jp = 0, jg = recombVars[ll].NumGhosts();
               jp < recombVars[ll].NumJ(); jg++, jp++) {
            for (auto ip = 0, ig = recombVars[ll].NumGhosts();
                 ip < recombVars[ll].NumI(); ig++, ip++) {
              dumArr(ip, jp, kp) =
                  recombVars[ll].State(ig, jg, kg).Omega();
            }
          }
        }
      } else if (var == "wallDistance") {
        for (auto kp = 0, kg = recombVars[ll].NumGhosts();
             kp < recombVars[ll].NumK(); kg++, kp++) {
          for (auto jp = 0, jg = recombVars[ll].NumGhosts();
               jp < recombVars[ll].NumJ(); jg++, jp++) {
            for (auto ip = 0, ig = recombVars[ll].NumGhosts();
                 ip < recombVars[ll].NumI(); ig++, ip++) {
              dumArr(ip, jp, kp) =
                  recombVars[ll].WallDist(ig, jg, kg);
            }
          }
        }
      } else if (var == "velGrad_ux") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].VelGrad(ii, jj, kk).XX();
            }
          }
        }
      } else if (var == "velGrad_vx") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].VelGrad(ii, jj, kk).XY();
            }
          }
        }
      } else if (var == "velGrad_wx") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].VelGrad(ii, jj, kk).XZ();
            }
          }
        }
      } else if (var == "velGrad_uy") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].VelGrad(ii, jj, kk).YX();
            }
          }
        }
      } else if (var == "velGrad_vy") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].VelGrad(ii, jj, kk).YY();
            }
          }
        }
      } else if (var == "velGrad_wy") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].VelGrad(ii, jj, kk).YZ();
            }
          }
        }
      } else if (var == "velGrad_uz") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].VelGrad(ii, jj, kk).ZX();
            }
          }
        }
      } else if (var == "velGrad_vz") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].VelGrad(ii, jj, kk).ZY();
            }
          }
        }
      } else if (var == "velGrad_wz") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].VelGrad(ii, jj, kk).ZZ();
            }
          }
        }
      } else if (var == "tempGrad_x") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].TempGrad(ii, jj, kk).X();
            }
          }
        }
      } else if (var == "tempGrad_y") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].TempGrad(ii, jj, kk).Y();
            }
          }
        }
      } else if (var == "tempGrad_z") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].TempGrad(ii, jj, kk).Z();
            }
          }
        }
      } else if (var == "tkeGrad_x") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].TkeGrad(ii, jj, kk).X();
            }
          }
        }
      } else if (var == "tkeGrad_y") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].TkeGrad(ii, jj, kk).Y();
            }
          }
        }
      } else if (var == "tkeGrad_z") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].TkeGrad(ii, jj, kk).Z();
            }
          }
        }
      } else if (var == "sdrGrad_x") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].OmegaGrad(ii, jj, kk).X();
            }
          }
        }
      } else if (var == "sdrGrad_y") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].OmegaGrad(ii, jj, kk).Y();
            }
          }
        }
      } else if (var == "sdrGrad_z") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].OmegaGrad(ii, jj, kk).Z();
            }
          }
        }
      } else if (var == "resid_mass") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].Residual(ii, jj, kk, 0);
            }
          }
        }
      } else if (var == "resid_mom_x") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].Residual(ii, jj, kk, 1);
            }
          }
        }
      } else if (var == "resid_mom_y") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].Residual(ii, jj, kk, 2);
            }
          }
        }
      } else if (var == "resid_mom_z") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].Residual(ii, jj, kk, 3);
            }
          }
        }
      } else if (var == "resid_energy") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].Residual(ii, jj, kk, 4);
            }
          }
        }
      } else if (var == "resid_tke") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].Residual(ii, jj, kk, 5);
            }
          }
        }
      } else if (var == "resid_sdr") {
        for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
          for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
            for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
              dumArr(ii, jj, kk) = recombVars[ll].Residual(ii, jj, kk, 6);
            }
          }
        }
      } else {
        cerr << "ERROR: Variable " << var
             << " to write to function file is not defined!" << endl;
        exit(EXIT_FAILURE);
      }

      // write out dimensional variables -- loop over block length
      for (auto kk = 0; kk < recombVars[ll].NumK(); kk++) {
        for (auto jj = 0; jj < recombVars[ll].NumJ(); jj++) {
          for (auto ii = 0; ii < recombVars[ll].NumI(); ii++) {
            auto dumDouble = dumArr(ii, jj, kk);

            if (var == "density") {
              dumDouble *= inp.RRef();
            } else if (var == "vel_x") {
              dumDouble *= refSoS;
            } else if (var == "vel_y") {
              dumDouble *= refSoS;
            } else if (var == "vel_z") {
              dumDouble *= refSoS;
            } else if (var == "pressure") {
              dumDouble *= inp.RRef() * refSoS * refSoS;
            } else if (var == "sos") {
              dumDouble *= refSoS;
            } else if (var == "dt") {
              dumDouble /= refSoS * inp.LRef();
            } else if (var == "temperature") {
              dumDouble *= inp.TRef();
            } else if (var == "tke") {
              dumDouble *= refSoS * refSoS;
            } else if (var == "sdr") {
              dumDouble *= refSoS * refSoS * inp.RRef() / suth.MuRef();
            } else if (var == "wallDistance") {
              dumDouble *= inp.LRef();
            } else if (var.find("velGrad_") != string::npos) {
              dumDouble *= refSoS / inp.LRef();
            } else if (var.find("tempGrad_") != string::npos) {
              dumDouble *= inp.TRef() / inp.LRef();
            } else if (var.find("tkeGrad_") != string::npos) {
              dumDouble *= refSoS * refSoS / inp.LRef();
            } else if (var.find("sdrGrad_") != string::npos) {
              dumDouble *= refSoS * refSoS * inp.RRef() /
                  (suth.MuRef() * inp.LRef());
            } else if (var == "resid_mass") {
              dumDouble *= inp.RRef() * refSoS * inp.LRef() * inp.LRef();
            } else if (var.find("resid_mom_") != string::npos) {
              dumDouble *= inp.RRef() * refSoS * refSoS * inp.LRef() *
                  inp.LRef();
            } else if (var == "resid_energy") {
              dumDouble *= inp.RRef() * pow(refSoS, 3.0) * inp.LRef() *
                  inp.LRef();
            } else if (var == "resid_tke") {
              dumDouble *= inp.RRef() * pow(refSoS, 3.0) * inp.LRef() *
                  inp.LRef();
            } else if (var == "resid_sdr") {
              dumDouble *= inp.RRef() * inp.RRef() * pow(refSoS, 4.0) *
                  inp.LRef() * inp.LRef() / suth.MuRef();
            }

            outFile.write(reinterpret_cast<char *>(&dumDouble),
                          sizeof(dumDouble));
          }
        }
      }
    }
  }

  // close plot3d function file
  outFile.close();
}

// function to write out results file for ensight
void WriteRes(const input &inp, const int &iter) {
  // open results file
  const string fResPostfix = ".res";
  const string fEnd = "_center";
  const auto resName = inp.SimNameRoot() + fEnd + fResPostfix;
  ofstream resFile(resName, ios::out);

  const auto outFreq = inp.OutputFrequency();
  
  // check to see if file opened correctly
  if (resFile.fail()) {
    cerr << "ERROR: Results file " << resName << " did not open correctly!!!"
         << endl;
    exit(EXIT_FAILURE);
  }

  const string fPostFix = ".fun";
  const auto writeName = inp.SimNameRoot() + "_*" + fEnd + fPostFix;

  const auto outputVars = inp.OutputVariables();
  const auto hasVelVector = outputVars.find("vel_x") != outputVars.end() &&
      outputVars.find("vel_y") != outputVars.end() &&
      outputVars.find("vel_z") != outputVars.end();
      
  
  // write number of scalars and number of vectors
  const auto numScalar = inp.NumVarsOutput();
  auto numVector = hasVelVector ? 1 : 0;
  resFile << numScalar << "     " << numVector << "     " << 0 << endl;

  // write number of time points that there is solution data at
  auto numTime = iter / outFreq + 1;
  resFile << numTime << endl;

  // Write solution times or iteration numbers
  auto solTime = 0;
  auto count = 1;
  for (auto ii = 0; ii < numTime; ii++) {
    solTime += outFreq;
    if (count % 10 == 0) {
      resFile << endl;
    }
    resFile << solTime << "   ";
    count++;
  }
  resFile << endl;

  // Write starting iteration and iteration increment
  resFile << outFreq << "  " << outFreq << endl;

  // Write out scalar variables
  vector3d<int> vectorIndices(0, 0, 0);
  auto nvar = 0;
  for (auto &var : outputVars) {
    resFile << writeName << " F " << std::setfill('0') << setw(4) << nvar
            << " " << var << "\n";
    if (var == "vel_x") {
      vectorIndices.SetX(nvar);
    } else if (var == "vel_y") {
      vectorIndices.SetY(nvar);
    } else if (var == "vel_z") {
      vectorIndices.SetZ(nvar);
    }
    nvar++;
  }

  // Write out vector variables
  if (hasVelVector) {
    resFile << writeName << " F " << std::setfill('0') << setw(4)
            << vectorIndices.X() << " " << std::setfill('0') << setw(4)
            << vectorIndices.Y() << " " << std::setfill('0') << setw(4)
            << vectorIndices.Z() << " velocity\n";
  }
  
  // Close results file
  resFile.close();
}

// function to write out residual information
void WriteResiduals(const input &inp, genArray &residL2First,
                    const genArray &residL2, const resid &residLinf,
                    const double &matrixResid, const int &nn, const int &mm,
                    ostream &resFile) {
  // if first iteration write headers to residual file
  if (nn == 0 && mm == 0) {
    PrintHeaders(inp, resFile);
  }

  // write out column headers every 100 iterations to standard out
  if (nn % 100 == 0 && mm == 0) {
    PrintHeaders(inp, cout);
  }

  // print residuals to standard out
  PrintResiduals(inp, residL2First, residL2, residLinf, matrixResid, nn, mm,
                 cout);
  // print residuals to residual file
  PrintResiduals(inp, residL2First, residL2, residLinf, matrixResid, nn, mm,
                 resFile);
}

void PrintHeaders(const input &inp, ostream &os) {
  // write out column headers
  os << std::left << setw(7) << "Step" << setw(8) << "NL-Iter";
  if (inp.Dt() > 0.0) {
    os << std::left << setw(12) << "Time-Step";
  } else if (inp.CFL() > 0.0) {
    os << std::left << setw(12) << "CFL";
  }
  os << std::left << setw(12) << "Res-Mass" << setw(12)
     << "Res-Mom-X" << setw(12) << "Res-Mom-Y" << setw(12) << "Res-Mom-Z"
     << setw(12) << "Res-Energy";
  if (inp.IsTurbulent()) {
    os << std::left << setw(12) << "Res-Tke" << setw(12) << "Res-Omega";
  }
  os << std::left << setw(8) << "Max-Eqn" << setw(8)
     << "Max-Blk" << setw(8) << "Max-I" << setw(8)
     << "Max-J" << setw(8) << "Max-K" << setw(12) << "Max-Res"
     << setw(12) << "Res-Matrix" << endl;
}

// function to write out residual information
void PrintResiduals(const input &inp, genArray &residL2First,
                    const genArray &residL2, const resid &residLinf,
                    const double &matrixResid, const int &nn, const int &mm,
                    ostream &os) {
  // determine normalization
  if (nn == 0 && mm == 0) {  // if at first iteration, normalize by itself
    residL2First = residL2;
  // if within first 5 iterations reset normalization
  } else if ((nn < 5) && mm == 0) {
    for (auto cc = 0; cc < NUMVARS; cc++) {
      if (residL2[cc] > residL2First[cc]) {
        residL2First[cc] = residL2[cc];
      }
    }
  }

  // normalize residuals
  const auto resNormL2 = (residL2 + EPS) / (residL2First + EPS);

  os << std::left << setw(7) << nn << setw(8) << mm;
  if (inp.Dt() > 0.0) {
    os << std::left << setw(12) << setprecision(4) << std::scientific
       << inp.Dt();
  } else if (inp.CFL() > 0.0) {
    os << std::left << setw(12) << setprecision(4) << std::scientific
       << inp.CFL();
  }
  os << std::left << setw(12) << resNormL2[0] << setw(12) << resNormL2[1]
     << setw(12) << resNormL2[2] << setw(12) << resNormL2[3] << setw(12)
     << resNormL2[4];
  if (inp.IsTurbulent()) {
    os << std::left << setw(12) << resNormL2[5] << setw(12) << resNormL2[6];
  }
  os.unsetf(std::ios::fixed | std::ios::scientific);
  os << std::left << setw(8) << residLinf.Eqn() << setw(8)
     << residLinf.Block() << setw(8) << residLinf.ILoc() << setw(8)
     << residLinf.JLoc() << setw(8) << residLinf.KLoc() << setw(12)
     << setprecision(4) << std::scientific << residLinf.Linf() << setw(12)
     << matrixResid << endl;

  os.unsetf(std::ios::fixed | std::ios::scientific);
}

/*Function to take in a vector of split procBlocks and return a vector of joined
 * procblocks (in their original configuration before grid decomposition).*/
vector<procBlock> Recombine(const vector<procBlock> &vars,
                            const decomposition &decomp) {
  // vars -- vector of split procBlocks
  // decomp -- decomposition

  auto recombVars = vars;
  vector<boundarySurface> dumSurf;
  for (auto ii = decomp.NumSplits() - 1; ii >= 0; ii--) {
    // recombine blocks and resize vector
    recombVars[decomp.SplitHistBlkLower(ii)]
        .Join(recombVars[decomp.SplitHistBlkUpper(ii)], decomp.SplitHistDir(ii),
              dumSurf);
    recombVars.resize(recombVars.size() - 1);
  }

  return recombVars;
}

/*Function to take in indices from the recombined procBlocks and determine which
 * split procBlock index the cell is associated with.*/
int SplitBlockNumber(const vector<procBlock> &vars, const decomposition &decomp,
                     const int &blk, const int &ii, const int &jj,
                     const int &kk) {
  // vars -- vector of recombined procblocks
  // decomp -- decomposition data structure
  // blk -- block number
  // ii -- i index of cell in recombined block to find split block number
  // jj -- j index of cell in recombined block to find split block number
  // kk -- k index of cell in recombined block to find split block number

  // Get block dimensions (both lower and upper extents)
  vector<pair<vector3d<int>, vector3d<int>>> blkDims(vars.size());
  vector3d<int> initialLower(0, 0, 0);
  for (auto bb = 0U; bb < blkDims.size(); bb++) {
    vector3d<int> dims(vars[bb].NumI(), vars[bb].NumJ(), vars[bb].NumK());
    blkDims[bb].first = initialLower;
    blkDims[bb].second = dims;
  }

  auto ind = blk;

  // no splits, cell must be in parent block already
  if (decomp.NumSplits() == 0) {
    return ind;
  } else {  // cell is in lower split already
    for (auto ss = 0; ss < decomp.NumSplits(); ss++) {  // loop over all splits
      // wrong parent block - split won't effect search so use dummy value
      if (blk != decomp.ParentBlock(ss + vars.size())) {
        pair<vector3d<int>, vector3d<int>> dumBlk(initialLower, initialLower);
        blkDims.push_back(dumBlk);
      } else {
        // "split" blocks - change lower limits of block
        if (decomp.SplitHistDir(ss) == "i") {
          pair<vector3d<int>, vector3d<int>> splitBlk =
              blkDims[decomp.SplitHistBlkLower(ss)];
          splitBlk.first[0] += decomp.SplitHistIndex(ss);
          blkDims.push_back(splitBlk);
        } else if (decomp.SplitHistDir(ss) == "j") {
          pair<vector3d<int>, vector3d<int>> splitBlk =
              blkDims[decomp.SplitHistBlkLower(ss)];
          splitBlk.first[1] += decomp.SplitHistIndex(ss);
          blkDims.push_back(splitBlk);
        } else {  // direction is k
          pair<vector3d<int>, vector3d<int>> splitBlk =
              blkDims[decomp.SplitHistBlkLower(ss)];
          splitBlk.first[2] += decomp.SplitHistIndex(ss);
          blkDims.push_back(splitBlk);
        }

        // test to see if block is in upper split
        if (!(ii <= blkDims[decomp.SplitHistBlkUpper(ss)].second.X() &&
              jj <= blkDims[decomp.SplitHistBlkUpper(ss)].second.Y() &&
              kk <= blkDims[decomp.SplitHistBlkUpper(ss)].second.Z() &&
              ii >= blkDims[decomp.SplitHistBlkUpper(ss)].first.X() &&
              jj >= blkDims[decomp.SplitHistBlkUpper(ss)].first.Y() &&
              kk >= blkDims[decomp.SplitHistBlkUpper(ss)].first.Z())) {
          // cell not in upper split, but in lower split - found block index
          return decomp.SplitHistBlkLower(ss);
        } else {  // cell in upper split (and lower split)
          ind = decomp.SplitHistBlkUpper(ss);
        }
      }
    }
  }

  return ind;  // cell was in uppermost split for given parent block
}