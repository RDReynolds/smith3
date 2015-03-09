//
// SMITH3 - generates spin-free multireference electron correlation programs.
// Filename: main.cc
// Copyright (C) 2012 Toru Shiozaki
//
// Author: Toru Shiozaki <shiozaki@northwestern.edu>
// Maintainer: Shiozaki group
//
// This file is part of the SMITH3 package.
//
// The SMITH3 package is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The SMITH3 package is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the SMITH3 package; see COPYING.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//

#include <iostream>
#include <tuple>
#include <string>
#include <memory>
#include <algorithm>
#include <vector>
#include <list>
#include <cassert>
#include <array>
#include <sstream>
#include <initializer_list>

#include "constants.h"
#include "tensor.h"
#include "diagram.h"
#include "equation.h"

using namespace std;

//const string theory = "MP2";
string theory = "MRCI";

using namespace SMITH3::Prep;

tuple<vector<shared_ptr<Tensor>>, vector<shared_ptr<Tensor>>, vector<shared_ptr<Tensor>>> create_proj() {
  vector<shared_ptr<Tensor>> lp, lt, ls, td;
  array<string, 3> label = {{"c", "x", "a"}};

  int cnt = 0;
  for (auto& i : label) {
    for (auto& j : label) {
      for (auto& k : label) {
        for (auto& l : label) {
          // full CASPT2
          if (
#if 0
              // all correct in this block
              (l == "c" && k == "c" && j == "a" && i == "a") ||
#endif
#if 1
              (l == "x" && k == "c" && j == "a" && i == "a") ||
#endif
#if 1
              (l == "x" && k == "x" && j == "a" && i == "a") //||
#endif
#if 0
              (l == "c" && k == "c" && j == "x" && i == "a") ||
              (l == "c" && k == "c" && j == "x" && i == "x") ||
              (l == "x" && k == "c" && j == "x" && i == "x") ||
              (l == "x" && k == "x" && j == "x" && i == "a") ||
              (l == "c" && k == "x" && j == "x" && i == "a") || (l == "x" && k == "c" && j == "x" && i == "a")
#endif
            ) {
            stringstream ss; ss << cnt;
            lp.push_back(shared_ptr<Tensor>(new Tensor(ss.str(), {l, k, j, i})));
            td.push_back(shared_ptr<Tensor>(new Tensor("t2dagger", ss.str(), {l, k, j, i})));
            lt.push_back(shared_ptr<Tensor>(new Tensor("t2", ss.str(), {j, i, l, k})));
            ++cnt;
          }
        }
      }
    }
  }

  return tie(lp, lt, td);
};

int main() {

  // generate common header
  cout << header() << endl;

  vector<shared_ptr<Tensor>> proj_list, t_list, t_dagger;
  tie(proj_list, t_list, t_dagger) = create_proj();

  // make f and H tensors here
  vector<shared_ptr<Tensor>> hc  = {shared_ptr<Tensor>(new Tensor("h1", "", {"g", "g"}))};
  vector<shared_ptr<Tensor>> H   = {shared_ptr<Tensor>(new Tensor("v2", "", {"g", "g", "g", "g"}))};
  vector<shared_ptr<Tensor>> dum = {shared_ptr<Tensor>(new Tensor("proj", "e", {}))};

  cout << "  string theory=\"" << theory << "\";" << endl;
  cout << endl;

  for (auto& i : proj_list) cout << i->generate();
  for (auto& i : t_list)    cout << i->generate();
  for (auto& i : H)         cout << i->generate();
  for (auto& i : hc)        cout << i->generate();
  for (auto& i : dum)       cout << i->generate();
  for (auto& i : t_dagger)  cout << i->generate();
  cout << endl;

  // residual equations //
  shared_ptr<Equation> eq0(new Equation(theory, "ra", {dum, proj_list, hc, t_list}));
  shared_ptr<Equation> eq1(new Equation(theory, "rb", {dum, proj_list, H, t_list}, 0.5));
  eq0->merge(eq1);
  for (int i = 0; i != proj_list.size(); ++i) {
    stringstream ss, tt;
    ss << "ra" << i;
    tt << "rb" << i;
    shared_ptr<Equation> eq0m(new Equation(theory, ss.str(), {dum, vector<shared_ptr<Tensor>>{proj_list[i]}, vector<shared_ptr<Tensor>>{t_list[i]}, hc}, -1.0));
    shared_ptr<Equation> eq1m(new Equation(theory, tt.str(), {dum, vector<shared_ptr<Tensor>>{proj_list[i]}, vector<shared_ptr<Tensor>>{t_list[i]}, H},  -0.5));
    eq0->merge(eq0m);
    eq0->merge(eq1m);
  }
  eq0->set_tree_type("residual");
  cout << eq0->generate();

  shared_ptr<Equation> eq3(new Equation(theory, "sa", {dum, proj_list, hc}));
  shared_ptr<Equation> eq4(new Equation(theory, "sb", {dum, proj_list, H}, 0.5));
  eq3->merge(eq4);
  eq3->set_tree_type("residual", "source");
  cout << eq3->generate();

  shared_ptr<Equation> eq2(new Equation(theory, "rc", {dum, proj_list, t_list}));
  eq2->set_tree_type("residual", "norm");
  cout << eq2->generate();

  // done. generate the footer
  cout << footer_ci(eq0->tree_label(), eq3->tree_label(), eq2->tree_label()) << endl;


  return 0;
}


