//
// Author : Toru Shiozaki
// Date   : Jan 2012
//

#include "diagram.h"

using namespace std;


// copying diagram with the same connectivity and so on.
shared_ptr<Diagram> Diagram::copy() const {
  // mapping of indices and spins. map<old, new>
  map<shared_ptr<Index>, shared_ptr<Index> > indexmap;
  map<shared_ptr<Spin>, shared_ptr<Spin> > spinmap;

  // creates Diagram without any info
  shared_ptr<Diagram> out(new Diagram());
  list<shared_ptr<Op> > outop;

  // loop over operators
  for (auto iter = op().begin(); iter != op().end(); ++iter) {
    // cloning...
    shared_ptr<Op> a = (*iter)->copy();

    auto j = a->op().begin();
    for (auto i = (*iter)->op().begin(); i != (*iter)->op().end(); ++i, ++j) {
      auto s = indexmap.find(*get<0>(*i));
      if (s == indexmap.end()) {
        indexmap.insert(make_pair(*get<0>(*i), *get<0>(*j)));
      } else {
        *get<0>(*j) = s->second; // one of the operators has s->second...
      }
      auto z = spinmap.find((*iter)->rho(get<2>(*i)));
      if (z == spinmap.end()) {
        spinmap.insert(make_pair((*iter)->rho(get<2>(*i)), a->rho(get<2>(*j))));
      } else {
        a->set_rho(get<2>(*j), z->second); // one of the operators has z->second...
      }
      get<1>(*j) = get<1>(*i);
    }
    outop.push_back(a);
  }
  out->set_op(outop);
  out->set_fac(fac_);
  return out;
}


void Diagram::refresh_indices() {
  map<shared_ptr<Index>, int> dict;
  map<shared_ptr<Index>, int> done;
  map<shared_ptr<Spin>, int> spin;
  for (auto i = op_.begin(); i != op_.end(); ++i)
    (*i)->refresh_indices(dict, done, spin);
}


// this is not a const function because it refreshes the indices
void Diagram::print() {
  refresh_indices();
  cout << setw(4) << setprecision(1) << fixed <<  fac_ << " ";
  for (auto i = op_.begin(); i != op_.end(); ++i) (*i)->print();
  cout << endl;
}

void Diagram::print() const {
  cout << setw(4) << setprecision(1) << fixed <<  fac_ << " ";
  for (auto i = op_.begin(); i != op_.end(); ++i) (*i)->print();
  cout << endl;
}


bool Diagram::reduce_one_noactive(const int skip) {
  refresh_indices();

  bool found = false;
  // find the first dagger operator in list<Op>
  auto i = op_.begin();
  pair<shared_ptr<Index>*, shared_ptr<Spin>* > data; // safe because they are held by tensors
  for (; i != op_.end(); ++i) {
    // this simultaneously eliminates one entry. op_ is therefore modified here
    data = (*i)->first_dagger_noactive();
    if (data.first) break;
  }
  if (!data.first) return false;

  // skip until it comes to skip
  int cnt = 0;
  shared_ptr<Spin> newspin, oldspin;
  for (auto j = op_.begin(); j != op_.end(); ++j) {
    // cannot contract with self
    if (i == j) continue;
    // all possible contraction pattern taken for *j (returned as a list).
    if (cnt + (*j)->num_nodagger() > skip) {
      tuple<double,shared_ptr<Spin>,shared_ptr<Spin> > tmp = (*j)->contract(data, skip-cnt);
      fac_ *= get<0>(tmp);
      newspin = get<1>(tmp);
      oldspin = get<2>(tmp);
      found = true;
      break;
    } else {
      cnt += (*j)->num_nodagger();
    }
  }
  for (auto j = op_.begin(); j != op_.end(); ++j) {
    for (auto k = (*j)->rho().begin(); k != (*j)->rho().end(); ++k) {
      if (*k == oldspin) *k = newspin;
    } 
  }
  return found;
}


bool Diagram::valid() const {
  int out = 0;
  for (auto i = op_.begin(); i != op_.end(); ++i) {
    if (!(*i)->contracted()) ++out;
  }
  return out > 1;
}


bool Diagram::done() const {
  int out = 0;
  for (auto i = op_.begin(); i != op_.end(); ++i) {
    if (!(*i)->contracted()) ++out;
  }
  return out == 0; 
}


int Diagram::num_dagger() const {
  int out = 0;
  for (auto i = op_.begin(); i !=  op_.end(); ++i) out += (*i)->num_dagger();
  return out;
}
