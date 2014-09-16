/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2014 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef METRICS_METRICSRESOURCE_H_INCLUDED
#define METRICS_METRICSRESOURCE_H_INCLUDED

#include <ripple/json/json_value.h>
#include "MetricsImpl.h"

namespace ripple {

namespace metrics {

namespace impl {

const Json::Value
toJSON (const MetricsImpl::Clock::duration& v)
{
  return static_cast<int>(v.count());
}

const Json::Value
toJSON (const unsigned long& v)
{
  return static_cast<int>(v);
}

const Json::Value
toJSON (unsigned long& v)
{
  return static_cast<int>(v);
}

class MetricsResourceBase {
public:
  virtual const Json::Value
    value (
      const MetricsImpl::Clock::time_point& nearest = MetricsImpl::Clock::now ()
      ) const = 0;
  virtual const std::string
    name () const = 0;
  virtual const Json::Value
    history (
      const MetricsImpl::HistoryRange& range = MetricsImpl::HistoryRange ()
      ) const = 0;
};

template <class T>
class MetricsResource
  : public MetricsResourceBase {
public:
  MetricsResource (T* element)
    : MetricsResourceBase ()
    , m_element (element) {}

  virtual const Json::Value
    value (
      const MetricsImpl::Clock::time_point& nearest = MetricsImpl::Clock::now ()
      ) const override {
    return toJSON (m_element->value (nearest));
  }

  virtual const std::string
    name () const override {
    return m_element->name ();
  }

  virtual const Json::Value
    history (
      const MetricsImpl::HistoryRange& range = MetricsImpl::HistoryRange ()
      ) const override {

    Json::Value ret (Json::objectValue);
    const typename T::History h =
      m_element->history (range.start (), range.end ());

    for(auto i = h.cbegin (); i != h.cend (); i++) {
      MetricsImpl::Clock::time_point mark = i->first;
      std::chrono::seconds age =
        std::chrono::duration_cast<std::chrono::seconds>(range.end () - mark);
      ret[std::to_string (age.count ())] = toJSON (i->second);
    }

    return ret;
  }

private:
  T* m_element;
};

class MetricsResourceListBase {
public:
  MetricsResourceListBase (const MetricsImpl* metrics)
    : m_metrics (metrics) {}
  virtual const Json::Value
    history (
      const MetricsImpl::HistoryRange& range = MetricsImpl::HistoryRange ()
      ) const = 0;
  virtual const Json::Value
    list () const = 0;
  virtual std::unique_ptr<MetricsResourceBase>
    getNamedResource (const std::string& name) const = 0;

protected:
  const MetricsImpl* m_metrics;
};

template <typename T>
class MetricsResourceList 
  : public MetricsResourceListBase {
public:
  MetricsResourceList (const MetricsImpl* metrics)
    : MetricsResourceListBase (metrics)
    , m_list (metrics->getMetricStore<T> ()) {}

  const Json::Value
    history (
      const MetricsImpl::HistoryRange& range = MetricsImpl::HistoryRange ()
      ) const override {
    Json::Value ret (Json::objectValue);
    for(auto i = m_list.cbegin (); i != m_list.cend (); i++) {
      MetricsResource<T> res (*i);
      ret[res.name ()] = res.history (range);
    }
    return ret;
  }

  const Json::Value
    list () const override {
    Json::Value ret (Json::arrayValue);
    for(auto i = m_list.cbegin (); i != m_list.cend (); i++) {
      ret.append ((*i)->name ());
    }
    return ret;
  }

  std::unique_ptr<MetricsResourceBase>
    getNamedResource (
      const std::string& name) const override {
    for(auto i = m_list.cbegin (); i != m_list.cend (); i++) {
      if ((*i)->name () == name)
        return std::make_unique<MetricsResource<T>> (*i);
    }
    // FIXME: raise exception?
    return nullptr;
  }
private:
  const std::forward_list<T*> m_list;
};

} //namespace impl

} //namespace metrics

} // namespace ripple

#endif //METRICS_METRICSRESOURCE_H_INCLUDED
