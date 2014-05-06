#pragma once
namespace Redis {
    class KeyHolder {
        enum class DataType { PTR, REFS };
        union Data {
            friend class KeyHolder;
            typedef std::vector<std::reference_wrapper<const std::string> > RefVec;
            const std::vector<std::string>* vector_ptr;
            const RefVec vector_of_ref;
        public:
            ~Data() {}
            template <class Iter>
            Data(Iter begin, Iter end) : vector_of_ref(begin, end) {}
            Data(const std::vector<std::string>& keys) : vector_ptr(&keys) {}
            Data(const Data& other) = delete;
            Data& operator=(const Data& other) = delete;
        };
        Data data;
        DataType type;
    public:
        KeyHolder(const std::vector<std::string>& keys) : data(keys), type(DataType::PTR) {
            redis_assert(!keys.empty());
        }
        template <class Iter>
        KeyHolder(const std::pair<Iter, Iter>& iter_pair) : data(iter_pair.first, iter_pair.second), type(DataType::REFS) {
            redis_assert(!data.vector_of_ref.empty());
        }
        template <class Container>
        KeyHolder(const Container& keys) : data(keys.begin(), keys.end()), type(DataType::REFS) {
            redis_assert(!keys.empty());
        }


        const std::string& operator[](size_t sz) const {
            if(type == DataType::PTR) {
                return data.vector_ptr->operator[](sz);
            }
            return data.vector_of_ref[sz];
        }
        size_t size() const {
            if(type == DataType::PTR) {
                return data.vector_ptr->size();
            }
            return data.vector_of_ref.size();
        }
    };

    class ValueHolder {
        enum class DataType { EMPTY, REFS };
        union Data {
            friend class ValueHolder;
            struct {
                std::vector<std::reference_wrapper<std::string> > v;
                size_t cur_size;
            } vector_of_ref;
            struct {
                std::function<void(const std::string&)> push_back_f;
                std::function<void(std::string&&)> move_push_back_f;
            } push_fns;
            Data(const std::function<void(const std::string&)>& pb_fun, const std::function<void(std::string&&)>& mv_pb_fun) :
                    push_fns({pb_fun, mv_pb_fun})
            {}
            template <class Iter>
            Data(const std::pair<Iter, Iter> its) : vector_of_ref({{its.first, its.second},0}) {}
            ~Data(){}
        };

        Data data;
        DataType type;

    public:
        ValueHolder(std::vector<std::string>& keys) :
                data([&keys](const std::string& val){keys.push_back(val);}, [&keys](std::string&& val){keys.push_back(std::move(val));}),
                type(DataType::EMPTY) {
            keys.clear();
        }

        template <class Container>
        ValueHolder(Container& keys) :
                data([&keys](const std::string& val){keys.push_back(val);}, [&keys](std::string&& val){keys.push_back(std::move(val));}),
                type(DataType::EMPTY) {
            keys.clear();
        }

        template <class Iter>
        ValueHolder(const std::pair<Iter, Iter>& iter_pair) : data(iter_pair), type(DataType::REFS) {
        }

        void push_back(const std::string& val) {
            if(type == DataType::EMPTY) {
                data.push_fns.push_back_f(val);
            }
            else if(type == DataType::REFS) {
                size_t& sz = data.vector_of_ref.cur_size;
                auto& vec = data.vector_of_ref.v;
                redis_assert(sz < vec.size());
                vec[sz].get() = val;
                sz++;
            }
            else {
                redis_assert_unreachable();
            }
        }

        void push_back(std::string&& val) {
            if(type == DataType::EMPTY) {
                data.push_fns.move_push_back_f(std::move(val));
            }
            else if(type == DataType::REFS) {
                size_t& sz = data.vector_of_ref.cur_size;
                auto& vec = data.vector_of_ref.v;
                redis_assert(sz < vec.size());
                vec[sz].get() = std::move(val);
                sz++;
            }
            else {
                redis_assert_unreachable();
            }
        }
    };

    template <class PairIter>
    struct KeyGetter {
        const std::string& operator()(const PairIter& it) const{
            return it->first;
        }
    };

    template <class PairIter>
    struct ValGetter {
        std::string& operator()(const PairIter& it) {
            return it->second;
        }
    };
    template <class PairIter, class Getter , class Value>
    class Iter : public std::iterator<std::input_iterator_tag, PairIter> {
        PairIter pair_iter;
        Getter g;
    public:
        Iter(const PairIter& it) :
                pair_iter(it)
        {}

        Value& operator*() {
            return g(pair_iter);
        }

        Value* operator->() const {
            return &(g(pair_iter));
        }

        Iter& operator++() {
            pair_iter++;
            return *this;
        }

        Iter operator++(int) {
            Iter tmp = *this;
            pair_iter++;
            return tmp;
        }

        Iter& operator--() {
            pair_iter--;
            return *this;
        }

        Iter operator--(int) {
            Iter tmp = *this;
            pair_iter--;
            return tmp;
        }

        bool operator==(const Iter& other) const {
            return pair_iter == other.pair_iter;
        }

        bool operator!=(const Iter& other) const {
            return pair_iter != other.pair_iter;
        }
    };
    template <class PairIter>
    using KeyIter = Iter<PairIter, KeyGetter<PairIter>, const std::string>;
    template <class PairIter>
    using ValIter = Iter<PairIter, ValGetter<PairIter>, std::string>;

    class KVHolder {
        friend class Connection;

        KeyHolder k;
        ValueHolder v;

    public:
        template <class Container, class Iterator = typename Container::iterator >
        KVHolder(Container& c) :
                k(std::make_pair(KeyIter<Iterator>(c.begin()), KeyIter<Iterator>(c.end()))),
                v(std::make_pair(ValIter<Iterator>(c.begin()), ValIter<Iterator>(c.end())))
        {}
    };

    class KKHolder {
        friend class Connection;

        KeyHolder k1;
        KeyHolder k2;

    public:
        template <class Container, class Iterator = typename Container::iterator >
        KKHolder(Container& c) :
                k1(std::make_pair(KeyIter<Iterator>(c.begin()), KeyIter<Iterator>(c.end()))),
                k2(std::make_pair(ValIter<Iterator>(c.begin()), ValIter<Iterator>(c.end())))
        {}
    };

}