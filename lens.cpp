#include <iostream>
#include <tuple>

// Implementation based off of https://www.youtube.com/watch?v=3kduOmZ2Wxw

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Show
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
struct Foo {
    std::tuple<int, int> bar;
    char baz;
};

void show(const std::tuple<int, int>& tup) {
    std::cout << "(" << std::get<0>(tup) << ", " << std::get<1>(tup) <<  ")";
}

void show(char c) {
    std::cout << "'" << c << "'";
}

void show(int i) {
    std::cout << i;
}

void show(const Foo& foo) {
    std::cout << "Foo: { bar: ";
    show(foo.bar);
    std::cout << ", baz: ";
    show(foo.baz);
    std::cout << " }";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Utiltity Types
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// type deduction for std::forward
template <class T>
decltype(auto) forward(T&& t) { return std::forward<T>(t); }

// Implementation of bind_front without a copy
template <class F, class... As>
auto bindF(F&& f, As&&... as) {
    return [&] (auto&&... bs) mutable -> decltype(auto) { 
        return forward(f)(forward(as)..., forward(bs)...); 
    };
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Identity Function and Functor
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
constexpr auto id() { return [](auto&& a) { return forward(a); }; };

template <class A>
struct Identity { A runIdentity; };

template <class A>
Identity(A&&) -> Identity<A>;

template <class A, class F>
constexpr auto fmap(F f, Identity<A>& a) { return Identity{ f(a.runIdentity) }; }

template <class A, class F>
constexpr auto fmap(F f, const Identity<A>& a) { return Identity{ f(a.runIdentity) }; }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Const Function and Functor
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template <class A>
constexpr auto constant(A&& a) { 
    return [&](auto&&...) mutable { return forward(a); }; 
};

template <class A>
struct Const { A runConst; };

template <class A>
Const(A&&) -> Const<A>;

template <class A, class F>
constexpr Const<A> fmap(F, Const<A>& c) { return c; }

template <class A, class F>
constexpr Const<A> fmap(F, const Const<A>& c) { return c; }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Lens
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template <class SA, class SAS>
struct Lens {
    SA sa;
    SAS sas;

    template <class AFA, class S> 
    constexpr decltype(auto) operator()(AFA&& pure, S&& record) const { 
        return fmap(bindF(sas, forward(record)), pure(sa(forward(record))));
    };
};

template <class SA, class SAS>
Lens(SA&&, SAS&&) -> Lens<SA, SAS>;

// Composition operator for generic (C++) Functor objects
// TODO: add concept to require f and g Lens and output a Lens?
template <class F, class G>
constexpr decltype(auto) operator|(F&& f, G&& g) {
    return [&] (auto&& b, auto&& a) -> decltype(auto) { 
        return forward(f)(bindF(forward(g), forward(b)), forward(a)); 
    };  
}

template <class S, class L>
constexpr decltype(auto) view(const L& l, S&& s) {
    auto make_const = [](auto&& a) { return Const{forward(a)}; };
    return l(make_const, forward(s)).runConst;
}

template <class S, class A, class L>
constexpr decltype(auto) set(const L& l, A&& a, S&& s) {
    auto const_id = constant(Identity{forward(a)});
    return l(const_id, forward(s)).runIdentity;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Example
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

constexpr auto bar = Lens{
    [](Foo& m) -> std::tuple<int, int>& { return m.bar; },
    [](Foo& m, std::tuple<int, int> x) -> Foo& {
        m.bar = x;
        return m;
    }
};

constexpr auto baz = Lens{
    [](Foo& m) -> char& { return m.baz; },
    [](Foo& m, char x) -> Foo& {
        m.baz = x;
        return m;
    }
};

constexpr auto first = Lens{
    [](const std::tuple<int, int>& x) -> const int& { return std::get<0>(x); }, 
    [](std::tuple<int, int>& x, int y) -> std::tuple<int, int>& {
        std::get<0>(x) = y;
        return x;
    }
};

constexpr auto second = Lens{
    [](const std::tuple<int, int>& x) -> decltype(auto) { return std::get<1>(x); }, 
    [](std::tuple<int, int>& x, int y) -> std::tuple<int, int>& {
        std::get<1>(x) = y;
        return x;
    }
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
auto main() -> int {

    int& (*my_fptr)(std::tuple<int, int>&) = &std::get<0>;

    Foo myFoo{{10, 2}, 'a'};

    show(view(bar | first, myFoo));
    std::cout << std::endl;
    

    show(myFoo);
    std::cout << std::endl;

    set(bar | second, 15, myFoo);
    set(baz, 'c', myFoo);
    show(myFoo);

    return view(bar | second, myFoo);
}
