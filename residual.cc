//
// SMITH3 - generates spin-free multireference electron correlation programs.
// Filename: residual.cc
// Copyright (C) 2013 Matthew MacLeod
//
// Author: Matthew MacLeod <matthew.macleod@northwestern.edu>
// Maintainer: Shiozaki group
//
// This file is part of the SMITH3 package.
//
// The SMITH3 package is free software; you can redistribute it and\/or modify
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


#include "residual.h"
#include <algorithm>

using namespace std;
using namespace smith;


// local functions... (not a good practice...) >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
static string merge__(vector<string> array) {
  stringstream ss;
  vector<string> done;
  for (auto& label : array) {
    size_t found = label.find("dagger");
    if (found != string::npos) {
      string tmp(label.begin(), label.begin() + found);
      label = tmp;
    }
    if (find(done.begin(), done.end(), label) != done.end()) continue;
    done.push_back(label);
    if (label == "f1" || label == "v2" || label == "h1") label = "this->" + label + "_";
    ss << (label != array.front() ? ", " : "") << ((label == "proj") ? "r" : label);
  }
  return ss.str();
}
static string merge__(list<string> array) { return merge__(vector<string>(array.begin(), array.end())); }
// local functions... (not a good practice...) <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<


pair<string, string> Residual::create_target(const string indent, const int i) const {
  stringstream ss;
  stringstream tt;

  tt << "template <typename T>" << endl;
  tt << "class Task0 : public Task<T> {" << endl;
  tt << "  protected:" << endl;
  tt << "    std::shared_ptr<Tensor<T>> r_;" << endl;
  tt << "    IndexRange closed_;" << endl;
  tt << "    IndexRange active_;" << endl;
  tt << "    IndexRange virt_;" << endl;
  tt << "" << endl;
  tt << "    void compute_() {" << endl;
  tt << "      r_->zero();" << endl;
  tt << "    };  " << endl;
  tt << "" << endl;
  tt << "  public:" << endl;
  tt << "    Task0(std::vector<std::shared_ptr<Tensor<T>>> t) : Task<T>() {" << endl;
  tt << "      r_ =  t[0];" << endl;
  tt << "    };  " << endl;
  tt << "    ~Task0() {}; " << endl;
  tt << "};" << endl << endl;
  
  ss << indent << "std::vector<std::shared_ptr<Tensor<T>>> tensor0 = {r};" << endl;
  ss << indent << "std::shared_ptr<Task0<T>> task0(new Task0<T>(tensor0));" << endl;
  ss << indent << "queue_->add_task(task0);" << endl << endl;

  return make_pair(ss.str(), tt.str());
}


string Residual::generate_task(const string indent, const int ip, const int ic, const vector<string> op, const string scalar, const int i0) const {
  stringstream ss;
  ss << indent << "std::vector<std::shared_ptr<Tensor<T>>> tensor" << ic << " = {" << merge__(op) << "};" << endl;
  ss << indent << "std::shared_ptr<Task" << ic << "<T>> task"
               << ic << "(new Task" << ic << "<T>(tensor" << ic << ", pindex" << (scalar.empty() ? "" : ", this->e0_") << "));" << endl;

  if (parent_) {
    assert(parent_->parent());
    ss << indent << "task" << ip << "->add_dep(task" << ic << ");" << endl;
    ss << indent << "task" << ic << "->add_dep(task0);" << endl;
  } else {
    assert(depth() == 0);
    ss << indent << "task" << ic << "->add_dep(task0);" << endl;
  }
  ss << indent << "queue_->add_task(task" << ic << ");" << endl;
  ss << endl;
  return ss.str();
}


string Residual::generate_compute_header(const int ic, const list<shared_ptr<const Index>> ti, const vector<shared_ptr<Tensor>> tensors, const bool no_outside) const {
  const int ninptensors = tensors.size()-1;

  bool need_e0 = false;
  for (auto& s : tensors)
    if (!s->scalar().empty()) need_e0 = true;

  const int nindex = ti.size();
  stringstream tt;
  tt << "template <typename T>" << endl;
  tt << "class Task" << ic << " : public Task<T> {" << endl;
  tt << "  protected:" << endl;
  // if index is empty give dummy arg
  tt << "    class Task_local : public SubTask<" << (ti.empty() ? 1 : nindex) << "," << ninptensors << ",T> {" << endl;
  tt << "      protected:" << endl;
  tt << "        const std::array<std::shared_ptr<const IndexRange>,3> range_;" << endl << endl;
 
  tt << "        const Index& b(const size_t& i) const { return this->block(i); }" << endl;
  tt << "        const std::shared_ptr<const Tensor<T>>& in(const size_t& i) const { return this->in_tensor(i); }" << endl;
  tt << "        const std::shared_ptr<Tensor<T>>& out() const { return this->out_tensor(); }" << endl;
  if (need_e0)
    tt << "        const double e0_;" << endl;
  tt << endl;

  tt << "      public:" << endl;
  // if index is empty use dummy index 1 to subtask
  if (ti.empty()) {
    tt << "        Task_local(const std::array<std::shared_ptr<const Tensor<T>>," << ninptensors <<  ">& in, std::shared_ptr<Tensor<T>>& out," << endl;
    tt << "                   std::array<std::shared_ptr<const IndexRange>,3>& ran" << (need_e0 ? ", const double e" : "") << ")" << endl;
    tt << "          : SubTask<1," << ninptensors << ",T>(std::array<const Index, 1>(), in, out), range_(ran)" << (need_e0 ? ", e0_(e)" : "") << " { }" << endl;
  } else {
    tt << "        Task_local(const std::array<const Index," << nindex << ">& block, const std::array<std::shared_ptr<const Tensor<T>>," << ninptensors <<  ">& in, std::shared_ptr<Tensor<T>>& out," << endl;
    tt << "                   std::array<std::shared_ptr<const IndexRange>,3>& ran" << (need_e0 ? ", const double e" : "") << ")" << endl;
    tt << "          : SubTask<" << nindex << "," << ninptensors << ",T>(block, in, out), range_(ran)" << (need_e0 ? ", e0_(e)" : "") << " { }" << endl;
  }
  tt << endl;
  tt << "        void compute() override {" << endl;

  if (!no_outside) {
    list<shared_ptr<const Index>> ti_copy = ti;
    if (depth() == 0) {
      for (auto i = ti_copy.begin(), j = ++ti_copy.begin(); i != ti_copy.end(); ++i, ++i, ++j, ++j)
        swap(*i, *j);
    }

    int cnt = 0;
    for (auto i = ti_copy.rbegin(); i != ti_copy.rend(); ++i) 
      tt << "          const Index " << (*i)->str_gen() << " = b(" << cnt++ << ");" << endl;
    tt << endl;
  }

  return tt.str();
}


string Residual::generate_compute_footer(const int ic, const list<shared_ptr<const Index>> ti, const vector<shared_ptr<Tensor>> tensors) const {
  const int ninptensors = tensors.size()-1;
  assert(ninptensors > 0);
  bool need_e0 = false;
  for (auto& s : tensors)
    if (!s->scalar().empty()) need_e0 = true;

  stringstream tt;
  tt << "        }" << endl;
  tt << "    };" << endl;
  tt << "" << endl;
  tt << "    std::vector<std::shared_ptr<Task_local>> subtasks_;" << endl;
  tt << "" << endl;

  tt << "    void compute_() override {" << endl;
  tt << "      for (auto& i : subtasks_) i->compute();" << endl;
  tt << "    }" << endl << endl; 

  tt << "  public:" << endl;
  tt << "    Task" << ic << "(std::vector<std::shared_ptr<Tensor<T>>> t,  std::array<std::shared_ptr<const IndexRange>,3> range" << (need_e0 ? ", const double e" : "") << ") : Task<T>() {" << endl;
  tt << "      std::array<std::shared_ptr<const Tensor<T>>," << ninptensors << "> in = {{";
  for (auto i = 1; i < ninptensors + 1; ++i)
    tt << "t[" << i << "]" << (i < ninptensors ? ", " : "");
  tt << "}};" << endl << endl;

  // over original outermost indices
  if (!ti.empty()) {
    tt << "      subtasks_.reserve("; 
    for (auto i = ti.begin(); i != ti.end(); ++i) {
      if (i != ti.begin()) tt << "*";
      tt << (*i)->generate_range() << "->nblock()";
    }
    tt << ");" << endl;
  }
  // loops 
  string indent = "      ";
  for (auto i = ti.begin(); i != ti.end(); ++i, indent += "  ") 
    tt << indent << "for (auto& " << (*i)->str_gen() << " : *" << (*i)->generate_range() << ")" << endl;
  // add subtasks
  if (!ti.empty()) {
    tt << indent  << "subtasks_.push_back(std::shared_ptr<Task_local>(new Task_local(std::array<const Index," << ti.size() << ">{{";
    for (auto i = ti.rbegin(); i != ti.rend(); ++i) {
      if (i != ti.rbegin()) tt << ", ";
      tt << (*i)->str_gen();
    }
    tt << "}}, in, t[0], range" << (need_e0 ? ", e" : "") << ")));" << endl;
  } else { 
    tt << indent  << "subtasks_.push_back(std::shared_ptr<Task_local>(new Task_local(in, t[0], range" << (need_e0 ? ", e" : "") << ")));" << endl;
  }

  tt << "    };" << endl;
  tt << "    ~Task" << ic << "() {};" << endl;
  tt << "};" << endl << endl;
  return tt.str();
}


pair<string, string> Residual::generate_bc(const string indent, const shared_ptr<BinaryContraction> i) const {
  stringstream ss;
  stringstream tt;
  if (depth() != 0) {
    const string bindent = indent + "    ";
    string dindent = bindent; 
    tt << target_->generate_get_block(dindent, "o", "out()", true);
    tt << target_->generate_scratch_area(dindent, "o", "out()", true); // true means zero-out
    list<shared_ptr<const Index>> ti = depth() != 0 ? (i)->target_indices() : (i)->tensor()->index();

    // inner loop will show up here
    // but only if outer loop is not empty
    list<shared_ptr<const Index>> di = (i)->loop_indices();
    vector<string> close2;
    if (ti.size() != 0) {
      tt << endl;
      for (auto iter = di.rbegin(); iter != di.rend(); ++iter, dindent += "  ") {
        string index = (*iter)->str_gen();
        tt << dindent << "for (auto& " << index << " : *" << (*iter)->generate_range("_") << ") {" << endl;
        close2.push_back(dindent + "}");
      }
    } else {
      int cnt = 0;
      for (auto k = di.begin(); k != di.end(); ++k, cnt++) tt << dindent << "const Index " <<  (*k)->str_gen() << " = b(" << cnt << ");" << endl;
      tt << endl;
    }

    // retrieving tensor_
    tt << (i)->tensor()->generate_get_block(dindent, "i0", "in(0)");
    tt << (i)->tensor()->generate_sort_indices(dindent, "i0", "in(0)", di) << endl;
    // retrieving subtree_
    tt << (i)->next_target()->generate_get_block(dindent, "i1", "in(1)");
    tt << (i)->next_target()->generate_sort_indices(dindent, "i1", "in(1)", di) << endl;

    // call dgemm
    {
      pair<string, string> t0 = (i)->tensor()->generate_dim(di);
      pair<string, string> t1 = (i)->next_target()->generate_dim(di);
      if (t0.first != "" || t1.first != "") {
        tt << dindent << "dgemm_(\"T\", \"N\", ";
        string tt0 = t0.first == "" ? "1" : t0.first;
        string tt1 = t1.first == "" ? "1" : t1.first;
        string ss0 = t1.second== "" ? "1" : t1.second;
        tt << tt0 << ", " << tt1 << ", " << ss0 << "," << endl;
        tt << dindent << "       1.0, i0data_sorted, " << ss0 << ", i1data_sorted, " << ss0 << "," << endl
           << dindent << "       1.0, odata_sorted, " << tt0;
        tt << ");" << endl;
      } else {
        if (depth() != 1) throw logic_error("should not happen in residual case");
      }
    }

    if (ti.size() != 0) {
      for (auto iter = close2.rbegin(); iter != close2.rend(); ++iter)
        tt << *iter << endl;
      tt << endl;
    }
    // Inner loop ends here

    // sort buffer
    {
      tt << (i)->target()->generate_sort_indices_target(bindent, "o", di, (i)->tensor(), (i)->next_target());
    }
    // put buffer
    {
      string label = target_->label();
      // new interface requires indices for put_block
      tt << bindent << "out()->put_block(odata";
      list<shared_ptr<const Index>> ti = depth() != 0 ? (i)->target_indices() : (i)->tensor()->index();
      for (auto i = ti.rbegin(); i != ti.rend(); ++i) 
        tt << ", " << (*i)->str_gen();
      tt << ");" << endl;
    }
  } else {  // now at bc depth 0 
      // making residual vector...
      list<shared_ptr<const Index>> proj = (i)->ex_target_index();
      list<shared_ptr<const Index>> res;
      assert(!(proj.size() & 1));
      for (auto ii = proj.begin(); ii != proj.end(); ++ii, ++ii) {
        auto j = ii; ++j;
        res.push_back(*j);
        res.push_back(*ii);
      }
      shared_ptr<Tensor> residual(new Tensor(1.0, "r", res));
      vector<shared_ptr<Tensor>> op2 = { (i)->next_target() };
      tt << generate_compute_operators(indent, residual, op2, (i)->dagger());
  }


  return make_pair(ss.str(), tt.str());
}







