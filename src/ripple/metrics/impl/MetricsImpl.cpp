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

#include "MetricsImpl.h"

#include <ripple/http/Session.h>
#include <ripple/json/json_value.h>
#include <ripple/module/net/rpc/RPCUtil.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/compare.hpp>

#include "MetricsResource.h"

namespace ripple {

namespace metrics {

namespace impl {

//FIXME: This should throw an exception so we can return a proper HTTP error
static
const MetricsImpl::Clock::time_point
readTimeParam (
    const std::map<std::string, std::string>& params,
    const std::string name, const MetricsImpl::Clock::time_point& def)
{
    if (params.count (name)) {
        boost::posix_time::ptime time;
        try {
            time = boost::posix_time::time_from_string (params.at (name));
        } catch (...) {
            try {
                time = boost::posix_time::from_iso_string (params.at (name));
            } catch (...) {
                return def;
            }
        }
        auto now = boost::posix_time::second_clock::universal_time ();
        auto diff = now - time;
        return MetricsImpl::Clock::now () -
            std::chrono::seconds (diff.total_seconds ());
    }
    return def;
}

//FIXME: This should be a private method, but probably needs PIMPL to hide use
//of MetricsResourceListBase from MetricsResource.h
static
std::unique_ptr<MetricsResourceListBase>
resourceList (MetricsImpl* self, const std::string& sensorClass)
{
    if (sensorClass == "meter")
        return std::make_unique<MetricsResourceList<MetricsMeterImpl>> (self);
    else if (sensorClass == "gauge")
        return std::make_unique<MetricsResourceList<MetricsGaugeImpl>> (self);
    else if (sensorClass == "event")
        return std::make_unique<MetricsResourceList<MetricsEventImpl>> (self);
    else if (sensorClass == "counter")
        return std::make_unique<MetricsResourceList<MetricsCounterImpl>> (self);
    return nullptr; // FIXME: raise exception
}

MetricsCounterImpl::MetricsCounterImpl (const std::string& name,
                                        MetricsImpl::Ptr const& impl)
    : beast::insight::CounterImpl ()
    , ExposableMetricsElement (name, impl)
{
    m_impl->add (this);
}

MetricsCounterImpl::~MetricsCounterImpl ()
{
    m_impl->remove (this);
}

void
MetricsCounterImpl::increment (value_type v)
{
    addValue (value() + v);
}

MetricsCounterImpl& MetricsCounterImpl::operator= (MetricsCounterImpl const&)
{
    assert (false);
}

MetricsEventImpl::MetricsEventImpl (const std::string& name,
                                    MetricsImpl::Ptr const& impl)
    : beast::insight::EventImpl ()
    , ExposableMetricsElement (name, impl)
{
    m_impl->add (this);
}

MetricsEventImpl::~MetricsEventImpl ()
{
    m_impl->remove (this);
}

void
MetricsEventImpl::notify (value_type const& v)
{
    addValue (v);
}

MetricsEventImpl&
MetricsEventImpl::operator= (MetricsEventImpl const&)
{
    assert (false);
}

MetricsGaugeImpl::MetricsGaugeImpl (const std::string& name,
                                    MetricsImpl::Ptr const& impl)
    : beast::insight::GaugeImpl ()
    , ExposableMetricsElement (name, impl)
{
    m_impl->add (this);
}

MetricsGaugeImpl::~MetricsGaugeImpl ()
{
    m_impl->remove (this);
}

void
MetricsGaugeImpl::set (value_type v)
{
    addValue (v);
}

MetricsGaugeImpl&
MetricsGaugeImpl::operator= (MetricsGaugeImpl const&)
{
    assert (false);
}

void
MetricsGaugeImpl::increment (difference_type v)
{
    addValue (value () + v);
}

MetricsMeterImpl::MetricsMeterImpl (const std::string& name,
                                    MetricsImpl::Ptr const& impl)
    : beast::insight::MeterImpl ()
    , ExposableMetricsElement (name, impl)
{
    m_impl->add (this);
}

MetricsMeterImpl::~MetricsMeterImpl ()
{
    m_impl->remove (this);
}

MetricsMeterImpl&
MetricsMeterImpl::operator= (MetricsMeterImpl const&)
{
    assert (false);
}

void
MetricsMeterImpl::increment (value_type v)
{
    addValue (value () + v);
}

MetricsHookImpl::MetricsHookImpl (HandlerType const& handler,
                                  MetricsImpl::Ptr const& impl)
  : beast::insight::HookImpl ()
  , MetricsElementBase (impl)
  , m_handler (handler)
{
    m_impl->add (this);
}

MetricsHookImpl::~MetricsHookImpl ()
{
    m_impl->remove (this);
}

MetricsHookImpl&
MetricsHookImpl::operator= (MetricsHookImpl const&)
{
    assert (false);
}

void
MetricsHookImpl::handle ()
{
    m_handler ();
}

MetricsImpl::MetricsImpl (int portNum, beast::Journal journal)
    : m_server (*this, journal)
    , m_context (ripple::RippleSSLContext::createBare ())
{
    ripple::HTTP::Ports ports;
    ripple::HTTP::Port port;
    beast::IP::Endpoint ep (beast::IP::Endpoint::from_string ("0.0.0.0"));

    port.addr = ep.at_port (0);
    port.port = portNum;
    port.context = m_context.get ();

    ports.push_back (port);
    m_server.setPorts (ports);
}

MetricsImpl::~MetricsImpl ()
{
    m_server.stop ();
}

beast::insight::Hook
MetricsImpl::make_hook (beast::insight::HookImpl::HandlerType const& type)
{
    return beast::insight::Hook (
        std::make_shared<MetricsHookImpl> (type, shared_from_this ())
    );
}

beast::insight::Counter
MetricsImpl::make_counter (std::string const& counter)
{
    return beast::insight::Counter (std::make_shared<MetricsCounterImpl> (counter, shared_from_this ()));
}

beast::insight::Event
MetricsImpl::make_event (std::string const& event)
{
    return beast::insight::Event (std::make_shared<MetricsEventImpl> (event, shared_from_this ()));
}

beast::insight::Gauge
MetricsImpl::make_gauge (std::string const& gauge)
{
    return beast::insight::Gauge (std::make_shared<MetricsGaugeImpl> (gauge, shared_from_this ()));
}

beast::insight::Meter
MetricsImpl::make_meter (std::string const& meter)
{
    return beast::insight::Meter (std::make_shared<MetricsMeterImpl> (meter, shared_from_this ()));
}

void
MetricsImpl::onAccept (ripple::HTTP::Session& session)
{
}

void
MetricsImpl::onRequest (ripple::HTTP::Session& session)
{
    Json::FastWriter writer;
    Json::Value ret;
    std::string uri;

    // FIXME: Implement in some sort of timer
    for(auto i = m_hooks.begin (); i != m_hooks.end (); i++) {
        (*i)->handle ();
    }

    uri = session.message ().url ();

    // FIXME: Why yes, I am parsing a HTTP GET request by hand.
    std::vector<std::string> tokens;
    std::vector<std::string> query;
    std::map<std::string, std::string> params;
    boost::split (query, uri, boost::is_any_of ("?"));
    boost::split (tokens, query[0], boost::is_any_of ("/"));

    tokens.erase (tokens.begin ());

    if (query.size () > 1) {
        std::vector<std::string> getTokens;
        boost::split (getTokens, query[1], boost::is_any_of ("&"));
        for(auto i = getTokens.cbegin ();i != getTokens.cend (); i++) {
            std::vector<std::string> pTokens;
            boost::split (pTokens, *i, boost::is_any_of ("="));
            params[pTokens[0]] = pTokens[1];
        }
    }

    if (tokens[0] == "") { // "/"
        // FIXME: Implement fetching of the html page
        assert (false);
    } else if (tokens[0] == "metric") { // "/metric/"
        //FIXME: Should redirect to URLs that end with /
        if (tokens.size() >= 2 && tokens[1] != "") { // "/metric/meter/"
            if (tokens.size() >= 3 && tokens[2] != "") { // "/metric/meter/foo"
                ret = Json::Value (Json::objectValue);
                std::string sensorClass = tokens[1];
                std::string sensorName = tokens[2];
                auto list = resourceList (this, sensorClass);
                auto resource = list->getNamedResource (sensorName);
                Clock::time_point start (
                    readTimeParam (params, "start", Clock::time_point ())
                );
                Clock::time_point end (
                    readTimeParam (params, "end", Clock::now ())
                );

                if (resource)
                  ret = resource->history (HistoryRange (start, end));
            } else {
                ret = Json::Value (Json::objectValue);
                std::string sensorClass = tokens[1];
                auto list = resourceList (this, sensorClass);
                ret = list->list ();
            }
        } else {
            ret = Json::Value (Json::arrayValue);
            ret.append (Json::Value ("meter"));
            ret.append (Json::Value ("gauge"));
            ret.append (Json::Value ("event"));
            ret.append (Json::Value ("counter"));
        }
    } else {
        // FIXME: 404 page
        assert (false);
    }

    session.write (createResponse (200, writer.write (ret)));
}

std::string
MetricsImpl::createResponse (int code, std::string const& body)
{
    return ripple::HTTPReply (code, body);
}

void
MetricsImpl::onClose (ripple::HTTP::Session& session,
                      boost::system::error_code const& ec)
{
}

void
MetricsImpl::onStopped (ripple::HTTP::Server& server)
{
}

} // namespace impl

std::shared_ptr<beast::insight::Collector>
Metrics::New (int portNum,
              beast::Journal journal)
{
    return std::make_shared<impl::MetricsImpl> (portNum, journal);
}

} // namespace metrics

} // namespace ripple

