
//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "react/detail/Defs.h"

#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "Observer.h"
#include "react/detail/ReactiveBase.h"
#include "react/detail/ReactiveDomain.h"
#include "react/detail/graph/EventStreamNodes.h"

/*****************************************/ REACT_BEGIN /*****************************************/

enum class EventToken { token };

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Events
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E = EventToken
>
class Events : public Reactive<REACT_IMPL::EventStreamNode<D,E>>
{
protected:
    using NodeT = REACT_IMPL::EventStreamNode<D, E>;

public:
    using ValueT = E;

    Events() :
        Reactive()
    {
    }

    explicit Events(const std::shared_ptr<NodeT>& ptr) :
        Reactive(ptr)
    {
    }

    template <typename F>
    Events Filter(F&& f)
    {
        return react::Filter(*this, std::forward<F>(f));
    }

    template <typename F>
    Events Transform(F&& f)
    {
        return react::Transform(*this, std::forward<F>(f));
    }

    template <typename F>
    Observer<D> Observe(F&& f)
    {
        return react::Observe(*this, std::forward<F>(f));
    }
};

/******************************************/ REACT_END /******************************************/

/***************************************/ REACT_IMPL_BEGIN /**************************************/

template <typename D, typename L, typename R>
bool Equals(const Events<D,L>& lhs, const Events<D,R>& rhs)
{
    return lhs.Equals(rhs);
}

/****************************************/ REACT_IMPL_END /***************************************/

/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Eventsource
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E = EventToken
>
class EventSource : public Events<D,E>
{
private:
    using NodeT = REACT_IMPL::EventSourceNode<D, E>;

public:
    EventSource() :
        Events()
    {
    }

    explicit EventSource(const std::shared_ptr<NodeT>& ptr) :
        Events(ptr)
    {
    }

    template <typename V>
    void Emit(V&& v) const
    {
        D::AddInput(*std::static_pointer_cast<NodeT>(ptr_), std::forward<V>(v));
    }

    template <typename = std::enable_if<std::is_same<E,EventToken>::value>::type>
    void Emit() const
    {
        Emit(EventToken::token);
    }

    const EventSource& operator<<(const E& e) const
    {
        Emit(e);
        return *this;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// TempEvents
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E,
    typename TOp
>
class TempEvents : public Events<D,E>
{
protected:
    using NodeT = REACT_IMPL::EventOpNode<D,E,TOp>;

public:    
    TempEvents() :
        Events()
    {}

    explicit TempEvents(const std::shared_ptr<NodeT>& ptr) :
        Events(ptr)
    {}

    explicit TempEvents(std::shared_ptr<NodeT>&& ptr) :
        Events(std::move(ptr))
    {}

    TOp StealOp()
    {
        return std::move(std::static_pointer_cast<NodeT>(ptr_)->StealOp());
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MakeEventSource
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E>
auto MakeEventSource()
    -> EventSource<D,E>
{
    return EventSource<D,E>(
        std::make_shared<REACT_IMPL::EventSourceNode<D,E>>());
}

template <typename D>
auto MakeEventSource()
    -> EventSource<D,EventToken>
{
    return EventSource<D,EventToken>(
        std::make_shared<REACT_IMPL::EventSourceNode<D,EventToken>>());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Merge
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename TArg1,
    typename ... TArgs,
    typename E = TArg1,
    typename TOp = REACT_IMPL::EventMergeOp<E,
        REACT_IMPL::EventStreamNodePtr<D,TArg1>,
        REACT_IMPL::EventStreamNodePtr<D,TArgs> ...>
>
auto Merge(const Events<D,TArg1>& arg1, const Events<D,TArgs>& ... args)
    -> TempEvents<D,E,TOp>
{
    static_assert(sizeof...(TArgs) > 0,
        "react::Merge requires at least 2 arguments.");

    return TempEvents<D,E,TOp>(
        std::make_shared<REACT_IMPL::EventOpNode<D,E,TOp>>(
            arg1.GetPtr(), args.GetPtr() ...));
}

template
<
    typename TLeftEvents,
    typename TRightEvents,
    typename D = TLeftEvents::DomainT,
    typename TLeftVal = TLeftEvents::ValueT,
    typename TRightVal = TRightEvents::ValueT,
    typename E = TLeftVal,
    typename TOp = REACT_IMPL::EventMergeOp<E,
        REACT_IMPL::EventStreamNodePtr<D,TLeftVal>,
        REACT_IMPL::EventStreamNodePtr<D,TRightVal>>,
    class = std::enable_if<
        IsEvent<TLeftEvents>::value>::type,
    class = std::enable_if<
        IsEvent<TRightEvents>::value>::type
>
auto operator|(const TLeftEvents& lhs, const TRightEvents& rhs)
    -> TempEvents<D,E,TOp>
{
    return TempEvents<D,E,TOp>(
        std::make_shared<REACT_IMPL::EventOpNode<D,E,TOp>>(
            lhs.GetPtr(), rhs.GetPtr()));
}

template
<
    typename D,
    typename TLeftVal,
    typename TLeftOp,
    typename TRightVal,
    typename TRightOp,
    typename E = TLeftVal,
    typename TOp = REACT_IMPL::EventMergeOp<E,TLeftOp,TRightOp>
>
auto operator|(TempEvents<D,TLeftVal,TLeftOp>&& lhs, TempEvents<D,TRightVal,TRightOp>&& rhs)
    -> TempEvents<D,E,TOp>
{
    return TempEvents<D,E,TOp>(
        std::make_shared<REACT_IMPL::EventOpNode<D,E,TOp>>(
            lhs.StealOp(), rhs.StealOp()));
}

template
<
    typename D,
    typename TLeftVal,
    typename TLeftOp,
    typename TRightEvents,
    typename TRightVal = TRightEvents::ValueT,
    typename E = TLeftVal,
    typename TOp = REACT_IMPL::EventMergeOp<E,
        TLeftOp,
        REACT_IMPL::EventStreamNodePtr<D,TRightVal>>,
    class = std::enable_if<
        IsEvent<TRightEvents>::value>::type
>
auto operator|(TempEvents<D,TLeftVal,TLeftOp>&& lhs, const TRightEvents& rhs)
    -> TempEvents<D,E,TOp>
{
    return TempEvents<D,E,TOp>(
        std::make_shared<REACT_IMPL::EventOpNode<D,E,TOp>>(
            lhs.StealOp(), rhs.GetPtr()));
}

template
<
    typename TLeftEvents,
    typename D,
    typename TRightVal,
    typename TRightOp,
    typename TLeftVal = TLeftEvents::ValueT,
    typename E = TLeftVal,
    typename TOp = REACT_IMPL::EventMergeOp<E,
        REACT_IMPL::EventStreamNodePtr<D,TRightVal>,
        TRightOp>,
    class = std::enable_if<
        IsEvent<TLeftEvents>::value>::type
>
auto operator|(const TLeftEvents& lhs, TempEvents<D,TRightVal,TRightOp>&& rhs)
    -> TempEvents<D,E,TOp>
{
    return TempEvents<D,E,TOp>(
        std::make_shared<REACT_IMPL::EventOpNode<D,E,TOp>>(
            lhs.GetPtr(), rhs.StealOp()));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Filter
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E,
    typename FIn,
    typename F = std::decay<FIn>::type,
    typename TOp = REACT_IMPL::EventFilterOp<E,F,
        REACT_IMPL::EventStreamNodePtr<D,E>>
>
auto Filter(const Events<D,E>& src, FIn&& filter)
    -> TempEvents<D,E,TOp>
{
    return TempEvents<D,E,TOp>(
        std::make_shared<REACT_IMPL::EventOpNode<D,E,TOp>>(
            std::forward<FIn>(filter), src.GetPtr()));
}

template
<
    typename D,
    typename E,
    typename TOpIn,
    typename FIn,
    typename F = std::decay<FIn>::type,
    typename TOpOut = REACT_IMPL::EventFilterOp<E,F,TOpIn>
>
auto Filter(TempEvents<D,E,TOpIn>&& src, FIn&& filter)
    -> TempEvents<D,E,TOpOut>
{
    return TempEvents<D,E,TOpOut>(
        std::make_shared<REACT_IMPL::EventOpNode<D,E,TOpOut>>(
            std::forward<FIn>(filter), src.StealOp()));
}

template
<
    typename TEvents,
    typename F,
    class = std::enable_if<
        IsEvent<TEvents>::value>::type
>
auto operator&(TEvents&& src, F&& filter)
    -> decltype(Filter(std::forward<TEvents>(src), std::forward<F>(filter)))
{
    return Filter(std::forward<TEvents>(src), std::forward<F>(filter));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Transform
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E,
    typename FIn,
    typename F = std::decay<FIn>::type,
    typename TOp = REACT_IMPL::EventTransformOp<E,F,
        REACT_IMPL::EventStreamNodePtr<D,E>>
>
auto Transform(const Events<D,E>& src, FIn&& func)
    -> TempEvents<D,E,TOp>
{
    return TempEvents<D,E,TOp>(
        std::make_shared<REACT_IMPL::EventOpNode<D,E,TOp>>(
            std::forward<FIn>(func), src.GetPtr()));
}

template
<
    typename D,
    typename E,
    typename TOpIn,
    typename FIn,
    typename F = std::decay<FIn>::type,
    typename TOpOut = REACT_IMPL::EventTransformOp<E,F,TOpIn>
>
auto Transform(TempEvents<D,E,TOpIn>&& src, FIn&& func)
    -> TempEvents<D,E,TOpOut>
{
    return TempEvents<D,E,TOpOut>(
        std::make_shared<REACT_IMPL::EventOpNode<D,E,TOpOut>>(
            std::forward<FIn>(func), src.StealOp()));
}

template
<
    typename TEvents,
    typename F,
    class = std::enable_if<
        IsEvent<TEvents>::value>::type
>
auto operator->*(TEvents&& src, F&& func)
    -> decltype(Transform(std::forward<TEvents>(src), std::forward<F>(func)))
{
    return Transform(std::forward<TEvents>(src), std::forward<F>(func));
}

/******************************************/ REACT_END /******************************************/
