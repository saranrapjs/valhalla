#include "mjolnir/osmway.h"
#include "baldr/edgeinfo.h"
#include "mjolnir/util.h"

#include "midgard/logging.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/regex.hpp>

#include <iostream>

using namespace valhalla::baldr;

namespace {

constexpr uint32_t kMaxNodesPerWay = 65535;
constexpr uint8_t kUnlimitedOSMSpeed = std::numeric_limits<uint8_t>::max();
constexpr float kMaxOSMSpeed = 140.0f;

} // namespace

namespace valhalla {
namespace mjolnir {

// Set the number of nodes for this way.
void OSMWay::set_node_count(const uint32_t count) {
  if (count > kMaxNodesPerWay) {
    LOG_WARN("Exceeded max nodes per way: " + std::to_string(count));
    nodecount_ = static_cast<uint16_t>(kMaxNodesPerWay);
  } else {
    nodecount_ = static_cast<uint16_t>(count);
  }
}

// Sets the speed in KPH.
void OSMWay::set_speed(const float speed) {
  if (speed > kMaxOSMSpeed) {
    LOG_WARN("Exceeded max speed for way id: " + std::to_string(osmwayid_));
    speed_ = kMaxOSMSpeed;
  } else {
    speed_ = static_cast<unsigned char>(speed + 0.5f);
  }
}

// Sets the speed limit in KPH.
void OSMWay::set_speed_limit(const float speed_limit) {
  if (speed_limit == kUnlimitedOSMSpeed) {
    speed_limit_ = kUnlimitedOSMSpeed;
  } else if (speed_limit > kMaxOSMSpeed) {
    LOG_WARN("Exceeded max speed for way id: " + std::to_string(osmwayid_));
    speed_limit_ = kMaxOSMSpeed;
  } else {
    speed_limit_ = static_cast<unsigned char>(speed_limit + 0.5f);
  }
}

// Sets the backward speed in KPH.
void OSMWay::set_backward_speed(const float backward_speed) {
  if (backward_speed > kMaxOSMSpeed) {
    LOG_WARN("Exceeded max backward speed for way id: " + std::to_string(osmwayid_));
    backward_speed_ = kMaxOSMSpeed;
  } else {
    backward_speed_ = static_cast<unsigned char>(backward_speed + 0.5f);
  }
}

// Sets the backward speed in KPH.
void OSMWay::set_forward_speed(const float forward_speed) {
  if (forward_speed > kMaxOSMSpeed) {
    LOG_WARN("Exceeded max forward speed for way id: " + std::to_string(osmwayid_));
    forward_speed_ = kMaxOSMSpeed;
  } else {
    forward_speed_ = static_cast<unsigned char>(forward_speed + 0.5f);
  }
}

// Sets the truck speed in KPH.
void OSMWay::set_truck_speed(const float speed) {
  if (speed > kMaxOSMSpeed) {
    LOG_WARN("Exceeded max truck speed for way id: " + std::to_string(osmwayid_));
    truck_speed_ = kMaxOSMSpeed;
  } else {
    truck_speed_ = static_cast<unsigned char>(speed + 0.5f);
  }
}

// Sets the number of lanes
void OSMWay::set_lanes(const uint32_t lanes) {
  lanes_ = (lanes > kMaxLaneCount) ? kMaxLaneCount : lanes;
}

// Sets the number of backward lanes
void OSMWay::set_backward_lanes(const uint32_t backward_lanes) {
  backward_lanes_ = (backward_lanes > kMaxLaneCount) ? kMaxLaneCount : backward_lanes;
}

// Sets the number of forward lanes
void OSMWay::set_forward_lanes(const uint32_t forward_lanes) {
  forward_lanes_ = (forward_lanes > kMaxLaneCount) ? kMaxLaneCount : forward_lanes;
}

void OSMWay::set_layer(int8_t layer) {
  layer_ = layer;
}

void OSMWay::AddPronunciations(std::vector<std::string>& linguistics,
                               const UniqueNames& name_offset_map,
                               const uint32_t ipa_index,
                               const uint32_t nt_sampa_index,
                               const uint32_t katakana_index,
                               const uint32_t jeita_index,
                               const size_t name_tokens_size,
                               const size_t key) const {

  auto get_pronunciations = [](const std::vector<std::string>& pronunciation_tokens, const size_t key,
                               const baldr::PronunciationAlphabet verbal_type) {
    linguistic_text_header_t header{static_cast<uint8_t>(baldr::Language::kNone), 0,
                                    static_cast<uint8_t>(verbal_type), static_cast<uint8_t>(key)};
    std::string pronunciation;
    // TODO: We need to address the fact that a name/ref value could of been entered incorrectly
    // For example, name="XYZ Street;;ABC Street"
    // name:pronunciation="pronunciation1;pronunciation2;pronunciation3" So we check for
    // name_tokens_size == pronunciation_tokens.size() and we will not toss the second record in the
    // vector, but we should as it is blank.  We actually address with blank name in edgeinfo via
    // tossing the name if GraphTileBuilder::AddName returns 0.  Thought is we could address this now
    // in GetTagTokens and if we have a mismatch then don't add the pronunciations.  To date no data
    // has been found as described, but it could happen.  Address this issue with the Language
    // updates.
    for (const auto& t : pronunciation_tokens) {
      if (!t.size()) { // pronunciation is blank. skip and increment the index
        ++header.name_index_;
        continue;
      }
      header.length_ = t.size();
      pronunciation.append(std::string(reinterpret_cast<const char*>(&header), 3) + t);
      ++header.name_index_;
    }
    return pronunciation;
  };

  std::vector<std::string> pronunciation_tokens;
  if (ipa_index != 0) {
    pronunciation_tokens = GetTagTokens(name_offset_map.name(ipa_index));
    if (pronunciation_tokens.size() && name_tokens_size == pronunciation_tokens.size())
      linguistics.emplace_back(
          get_pronunciations(pronunciation_tokens, key, baldr::PronunciationAlphabet::kIpa));
  }

  if (nt_sampa_index != 0) {
    pronunciation_tokens = GetTagTokens(name_offset_map.name(nt_sampa_index));
    if (pronunciation_tokens.size() && name_tokens_size == pronunciation_tokens.size())
      linguistics.emplace_back(
          get_pronunciations(pronunciation_tokens, key, baldr::PronunciationAlphabet::kNtSampa));
  }

  if (katakana_index != 0) {
    pronunciation_tokens = GetTagTokens(name_offset_map.name(katakana_index));
    if (pronunciation_tokens.size() && name_tokens_size == pronunciation_tokens.size())
      linguistics.emplace_back(
          get_pronunciations(pronunciation_tokens, key, baldr::PronunciationAlphabet::kXKatakana));
  }

  if (jeita_index != 0) {
    pronunciation_tokens = GetTagTokens(name_offset_map.name(jeita_index));
    if (pronunciation_tokens.size() && name_tokens_size == pronunciation_tokens.size())
      linguistics.emplace_back(
          get_pronunciations(pronunciation_tokens, key, baldr::PronunciationAlphabet::kXJeita));
  }
}

void OSMWay::AddLanguages(std::vector<std::string>& linguistics,
                          const std::vector<baldr::Language>& token_languages,
                          const size_t key) const {
  if (token_languages.size() != 0) {
    std::string language;

    linguistic_text_header_t header{static_cast<uint8_t>(baldr::Language::kNone), 0,
                                    static_cast<uint8_t>(baldr::PronunciationAlphabet::kNone),
                                    static_cast<uint8_t>(key)};
    for (const auto& t : token_languages) {
      header.language_ = static_cast<uint8_t>(t);
      header.length_ = 0;
      language.append(std::string(reinterpret_cast<const char*>(&header), 3));
      ++header.name_index_;
    }
    linguistics.emplace_back(language);
  }
}

void OSMWay::ProcessNames(const UniqueNames& name_offset_map,
                          const std::vector<std::string>& default_languages,
                          const uint32_t name_index,
                          const uint32_t name_lang_index,
                          std::vector<std::string>& tokens,
                          std::vector<baldr::Language>& token_langs,
                          bool diff_names) const {

  std::vector<std::string> token_languages;
  std::vector<std::pair<std::string, std::string>> updated_token_languages;
  tokens = GetTagTokens(name_offset_map.name(name_index));
  token_languages = GetTagTokens(name_offset_map.name(name_lang_index));

  // remove any entries that are not in our country language list
  // then sort our names based on the list.
  if (default_languages.size() && tokens.size() == token_languages.size()) { // should always be equal
    for (size_t i = 0; i < token_languages.size(); i++) {
      if (std::find(default_languages.begin(), default_languages.end(), token_languages[i]) !=
          default_languages.end()) {
        updated_token_languages.emplace_back(tokens[i], token_languages[i]);
      }
    }

    std::unordered_map<std::string, uint32_t> lang_sort_order;
    for (size_t i = 0; i < default_languages.size(); i++)
      lang_sort_order[default_languages[i]] = i;

    auto cmp = [&lang_sort_order](const std::pair<std::string, std::string>& p1,
                                  const std::pair<std::string, std::string>& p2) {
      return lang_sort_order[p1.second] < lang_sort_order[p2.second];
    };

    std::sort(updated_token_languages.begin(), updated_token_languages.end(), cmp);
    std::vector<std::string> multilingual_names, multilingual_names_found, names_w_no_lang,
        supported_names;
    std::vector<baldr::Language> multilingual_langs_found, supported_langs;
    uint32_t names_w_no_lang_count = 0;

    for (size_t i = 0; i < updated_token_languages.size(); ++i) {
      if (updated_token_languages[i].second.empty()) {
        // multilingual name
        // name = Place Saint-Pierre - Sint-Pietersplein
        std::vector<std::string> temp_names = GetTagTokens(updated_token_languages[i].first, " - ");
        if (temp_names.size() >= 2)
          multilingual_names.insert(multilingual_names.end(), temp_names.begin(), temp_names.end());
        else {
          temp_names = GetTagTokens(updated_token_languages[i].first, " / ");
          if (temp_names.size() >= 2)
            multilingual_names.insert(multilingual_names.end(), temp_names.begin(), temp_names.end());
          else
            names_w_no_lang.emplace_back(updated_token_languages[i].first);
        }
      } else if (std::find(multilingual_names.begin(), multilingual_names.end(),
                           updated_token_languages[i].first) != multilingual_names.end()) {
        multilingual_names_found.emplace_back(updated_token_languages[i].first);
        multilingual_langs_found.emplace_back(stringLanguage(updated_token_languages[i].second));
      } else if (std::find(default_languages.begin(), default_languages.end(),
                           updated_token_languages[i].second) != default_languages.end()) {

        if (diff_names && std::find(names_w_no_lang.begin(), names_w_no_lang.end(),
                                    updated_token_languages[i].first) == names_w_no_lang.end())
          continue; // is right or left name.

        supported_names.emplace_back(updated_token_languages[i].first);
        supported_langs.emplace_back(stringLanguage(updated_token_languages[i].second));
      } else if (std::find(names_w_no_lang.begin(), names_w_no_lang.end(),
                           updated_token_languages[i].first) != names_w_no_lang.end()) {
        names_w_no_lang_count++;
      }
    }
    bool multi_names = (multilingual_names_found.size());
    bool default_names =
        (names_w_no_lang_count != 0 && names_w_no_lang_count >= names_w_no_lang.size());
    bool allowed_names = (supported_names.size() != 0);

    if (multi_names || default_names || allowed_names) {
      tokens.clear();
      if (multi_names) {
        tokens.insert(tokens.end(), multilingual_names_found.begin(), multilingual_names_found.end());
        token_langs.insert(token_langs.end(), multilingual_langs_found.begin(),
                           multilingual_langs_found.end());
      }
      if (default_names) {
        tokens.insert(tokens.end(), names_w_no_lang.begin(), names_w_no_lang.end());

        for (size_t i = 0; i < names_w_no_lang.size(); i++)
          token_langs.emplace_back(Language::kNone);
      }
      if (allowed_names) {
        tokens.insert(tokens.end(), supported_names.begin(), supported_names.end());
        token_langs.insert(token_langs.end(), supported_langs.begin(), supported_langs.end());
      }
    } else { // bail
      tokens.clear();
      for (size_t i = 0; i < updated_token_languages.size(); ++i) {
        if (updated_token_languages[i].second.empty()) {
          tokens.emplace_back(updated_token_languages[i].first);
        }
      }
    }
  }
}

// Get the names for the edge info based on the road class.
void OSMWay::GetNames(const std::string& ref,
                      const UniqueNames& name_offset_map,
                      const OSMPronunciation& pronunciation,
                      const std::vector<std::string>& default_languages,
                      const uint32_t name_index,
                      const uint32_t name_lang_index,
                      const uint32_t official_name_index,
                      const uint32_t official_name_lang_index,
                      const uint32_t alt_name_index,
                      const uint32_t alt_name_lang_index,
                      uint16_t& types,
                      std::vector<std::string>& names,
                      std::vector<std::string>& linguistics,
                      bool diff_names) const {

  uint16_t location = 0;
  types = 0;

  // Process motorway and trunk refs
  if ((ref_index_ != 0 || !ref.empty()) &&
      ((static_cast<RoadClass>(road_class_) == RoadClass::kMotorway) ||
       (static_cast<RoadClass>(road_class_) == RoadClass::kTrunk))) {
    std::vector<std::string> tokens;
    std::vector<std::string> pronunciation_tokens;

    if (!ref.empty()) {
      tokens = GetTagTokens(ref); // use updated refs from relations.
    } else {
      tokens = GetTagTokens(name_offset_map.name(ref_index_));
    }

    for (size_t i = 0; i < tokens.size(); ++i) {
      types |= static_cast<uint64_t>(1) << location;
      location++;
    }

    names.insert(names.end(), tokens.begin(), tokens.end());

    size_t key = names.size() - tokens.size();
    AddPronunciations(linguistics, name_offset_map, pronunciation.ref_pronunciation_ipa_index(),
                      pronunciation.ref_pronunciation_nt_sampa_index(),
                      pronunciation.ref_pronunciation_katakana_index(),
                      pronunciation.ref_pronunciation_jeita_index(), tokens.size(), key);
  }

  // Process name
  if (name_index != 0) {

    std::vector<std::string> tokens;
    std::vector<baldr::Language> token_langs;
    ProcessNames(name_offset_map, default_languages, name_index, name_lang_index, tokens, token_langs,
                 diff_names);

    location += tokens.size();

    names.insert(names.end(), tokens.begin(), tokens.end());

    size_t key = names.size() - tokens.size();

    AddPronunciations(linguistics, name_offset_map, pronunciation.name_pronunciation_ipa_index(),
                      pronunciation.name_pronunciation_nt_sampa_index(),
                      pronunciation.name_pronunciation_katakana_index(),
                      pronunciation.name_pronunciation_jeita_index(), tokens.size(), key);

    AddLanguages(linguistics, token_langs, key);
  }

  // Process non limited access refs
  if (ref_index_ != 0 && (static_cast<RoadClass>(road_class_) != RoadClass::kMotorway) &&
      (static_cast<RoadClass>(road_class_) != RoadClass::kTrunk)) {
    std::vector<std::string> tokens;

    if (!ref.empty()) {
      tokens = GetTagTokens(ref); // use updated refs from relations.
    } else {
      tokens = GetTagTokens(name_offset_map.name(ref_index_));
    }

    for (size_t i = 0; i < tokens.size(); ++i) {
      types |= static_cast<uint64_t>(1) << location;
      location++;
    }

    names.insert(names.end(), tokens.begin(), tokens.end());

    size_t key = names.size() - tokens.size();
    AddPronunciations(linguistics, name_offset_map, pronunciation.ref_pronunciation_ipa_index(),
                      pronunciation.ref_pronunciation_nt_sampa_index(),
                      pronunciation.ref_pronunciation_katakana_index(),
                      pronunciation.ref_pronunciation_jeita_index(), tokens.size(), key);
  }

  // Process alt_name
  if (alt_name_index != 0 && alt_name_index != name_index) {

    std::vector<std::string> tokens;
    std::vector<baldr::Language> token_langs;

    ProcessNames(name_offset_map, default_languages, alt_name_index, alt_name_lang_index, tokens,
                 token_langs, diff_names);
    location += tokens.size();

    names.insert(names.end(), tokens.begin(), tokens.end());

    size_t key = names.size() - tokens.size();
    AddPronunciations(linguistics, name_offset_map, pronunciation.alt_name_pronunciation_ipa_index(),
                      pronunciation.alt_name_pronunciation_nt_sampa_index(),
                      pronunciation.alt_name_pronunciation_katakana_index(),
                      pronunciation.alt_name_pronunciation_jeita_index(), tokens.size(), key);
    AddLanguages(linguistics, token_langs, key);
  }
  // Process official_name
  if (official_name_index != 0 && official_name_index != name_index &&
      official_name_index != alt_name_index_) {

    std::vector<std::string> tokens;
    std::vector<baldr::Language> token_langs;
    ProcessNames(name_offset_map, default_languages, official_name_index, official_name_lang_index,
                 tokens, token_langs, diff_names);
    location += tokens.size();

    names.insert(names.end(), tokens.begin(), tokens.end());

    size_t key = names.size() - tokens.size();
    AddPronunciations(linguistics, name_offset_map,
                      pronunciation.official_name_pronunciation_ipa_index(),
                      pronunciation.official_name_pronunciation_nt_sampa_index(),
                      pronunciation.official_name_pronunciation_katakana_index(),
                      pronunciation.official_name_pronunciation_jeita_index(), tokens.size(), key);
    AddLanguages(linguistics, token_langs, key);
  }
}

// Get the tagged names for an edge
void OSMWay::GetTaggedValues(const UniqueNames& name_offset_map,
                             const OSMPronunciation& pronunciation,
                             const size_t& names_size,
                             std::vector<std::string>& names,
                             std::vector<std::string>& linguistics) const {

  std::vector<std::string> tokens;

  auto encode_tag = [](TaggedValue tag) {
    return std::string(1, static_cast<std::string::value_type>(tag));
  };
  if (tunnel_name_index_ != 0) {
    // tunnel names
    auto tokens = GetTagTokens(name_offset_map.name(tunnel_name_index_));
    for (const auto& t : tokens) {
      names.emplace_back(encode_tag(TaggedValue::kTunnel) + t);
    }

    size_t key = (names_size + names.size()) - tokens.size();
    AddPronunciations(linguistics, name_offset_map,
                      pronunciation.tunnel_name_pronunciation_ipa_index(),
                      pronunciation.tunnel_name_pronunciation_nt_sampa_index(),
                      pronunciation.tunnel_name_pronunciation_katakana_index(),
                      pronunciation.tunnel_name_pronunciation_jeita_index(), tokens.size(), key);
  }

  if (layer_ != 0) {
    names.emplace_back(encode_tag(TaggedValue::kLayer) + static_cast<char>(layer_));
  }
}

} // namespace mjolnir
} // namespace valhalla
