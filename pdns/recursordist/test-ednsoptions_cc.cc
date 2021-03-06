#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_NO_MAIN

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <boost/test/unit_test.hpp>
#include <utility>

#include "dnsname.hh"
#include "dnswriter.hh"
#include "ednscookies.hh"
#include "ednsoptions.hh"
#include "ednssubnet.hh"
#include "iputils.hh"

/* extract a specific EDNS0 option from a pointer on the beginning rdLen of the OPT RR */
int getEDNSOption(char* optRR, size_t len, uint16_t wantedOption, char ** optionValue, size_t * optionValueSize);

BOOST_AUTO_TEST_SUITE(ednsoptions_cc)

static void getRawQueryWithECSAndCookie(const DNSName& name, const Netmask& ecs, const std::string& clientCookie, const std::string& serverCookie, std::vector<uint8_t>& query)
{
  DNSPacketWriter pw(query, name, QType::A, QClass::IN, 0);
  pw.commit();

  EDNSCookiesOpt cookiesOpt;
  cookiesOpt.client = clientCookie;
  cookiesOpt.server = serverCookie;
  string cookiesOptionStr = makeEDNSCookiesOptString(cookiesOpt);
  EDNSSubnetOpts ecsOpts;
  ecsOpts.source = ecs;
  string origECSOptionStr = makeEDNSSubnetOptsString(ecsOpts);
  DNSPacketWriter::optvect_t opts;
  opts.push_back(make_pair(EDNSOptionCode::COOKIE, cookiesOptionStr));
  opts.push_back(make_pair(EDNSOptionCode::ECS, origECSOptionStr));
  opts.push_back(make_pair(EDNSOptionCode::COOKIE, cookiesOptionStr));
  pw.addOpt(512, 0, 0, opts);
  pw.commit();
}

BOOST_AUTO_TEST_CASE(test_getEDNSOption) {
  DNSName name("www.powerdns.com.");
  Netmask ecs("127.0.0.1/32");
  vector<uint8_t> query;

  getRawQueryWithECSAndCookie(name, ecs, "deadbeef", "deadbeef", query);

  const struct dnsheader* dh = reinterpret_cast<struct dnsheader*>(query.data());
  size_t questionLen = query.size();
  unsigned int consumed = 0;
  DNSName dnsname = DNSName(reinterpret_cast<const char*>(query.data()), questionLen, sizeof(dnsheader), false, nullptr, nullptr, &consumed);

  size_t pos = sizeof(dnsheader) + consumed + 4;
  /* at least OPT root label (1), type (2), class (2) and ttl (4) + OPT RR rdlen (2) = 11 */
  BOOST_REQUIRE_EQUAL(ntohs(dh->arcount), 1);
  BOOST_REQUIRE(questionLen > pos + 11);
  /* OPT root label (1) followed by type (2) */
  BOOST_REQUIRE_EQUAL(query.at(pos), 0);
  BOOST_REQUIRE(query.at(pos+2) == QType::OPT);

  char* ecsStart = nullptr;
  size_t ecsLen = 0;
  int res = getEDNSOption(reinterpret_cast<char*>(query.data())+pos+9, questionLen - pos - 9, EDNSOptionCode::ECS, &ecsStart, &ecsLen);
  BOOST_CHECK_EQUAL(res, 0);

  EDNSSubnetOpts eso;
  BOOST_REQUIRE(getEDNSSubnetOptsFromString(ecsStart + 4, ecsLen - 4, &eso));

  BOOST_CHECK(eso.source == ecs);
}

BOOST_AUTO_TEST_CASE(test_getEDNSOptions) {
  DNSName name("www.powerdns.com.");
  Netmask ecs("127.0.0.1/32");
  vector<uint8_t> query;

  getRawQueryWithECSAndCookie(name, ecs, "deadbeef", "deadbeef", query);

  const struct dnsheader* dh = reinterpret_cast<struct dnsheader*>(query.data());
  size_t questionLen = query.size();
  unsigned int consumed = 0;
  DNSName dnsname = DNSName(reinterpret_cast<const char*>(query.data()), questionLen, sizeof(dnsheader), false, nullptr, nullptr, &consumed);

  size_t pos = sizeof(dnsheader) + consumed + 4;
  /* at least OPT root label (1), type (2), class (2) and ttl (4) + OPT RR rdlen (2) = 11 */
  BOOST_REQUIRE_EQUAL(ntohs(dh->arcount), 1);
  BOOST_REQUIRE(questionLen > pos + 11);
  /* OPT root label (1) followed by type (2) */
  BOOST_REQUIRE_EQUAL(query.at(pos), 0);
  BOOST_REQUIRE(query.at(pos+2) == QType::OPT);

  std::map<uint16_t, EDNSOptionView> options;
  int res = getEDNSOptions(reinterpret_cast<char*>(query.data())+pos+9, questionLen - pos - 9, options);
  BOOST_REQUIRE_EQUAL(res, 0);

  /* 3 EDNS options but two of them are EDNS Cookie, so we only keep one */
  BOOST_CHECK_EQUAL(options.size(), 2);

  auto it = options.find(EDNSOptionCode::ECS);
  BOOST_REQUIRE(it != options.end());
  BOOST_REQUIRE(it->second.content != nullptr);
  BOOST_REQUIRE_GT(it->second.size, 0);

  EDNSSubnetOpts eso;
  BOOST_REQUIRE(getEDNSSubnetOptsFromString(it->second.content, it->second.size, &eso));
  BOOST_CHECK(eso.source == ecs);

  it = options.find(EDNSOptionCode::COOKIE);
  BOOST_REQUIRE(it != options.end());
  BOOST_REQUIRE(it->second.content != nullptr);
  BOOST_REQUIRE_GT(it->second.size, 0);
}

BOOST_AUTO_TEST_SUITE_END()
