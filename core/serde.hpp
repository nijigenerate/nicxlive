#pragma once

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <optional>
#include <string>
#include <vector>

namespace nicxlive::core::serde {

using Fghj = boost::property_tree::ptree;
using SerdeException = std::optional<std::string>;

class InochiSerializer {
public:
    InochiSerializer() : current(&root) {}

    void putKey(const std::string& key) { pendingKey = key; }

    template <typename T>
    void putValue(const T& value) {
        if (!pendingKey.empty()) {
            current->put(pendingKey, value);
            pendingKey.clear();
        }
    }

    template <typename T>
    void serializeValue(const T& value) {
        // Generic fall-back for types with serialize method
        value.serialize(*this);
    }

    boost::property_tree::ptree::value_type listBegin() {
        stack.push_back(current);
        current = &pendingList;
        pendingList.clear();
        inList = true;
        return {};
    }

    void elemBegin() {
        if (!inList) return;
        pendingList.push_back({"", boost::property_tree::ptree{}});
        current = &pendingList.back().second;
    }

    void listEnd(boost::property_tree::ptree::value_type) {
        if (!stack.empty()) {
            current = stack.back();
            stack.pop_back();
        }
        if (!pendingKey.empty()) {
            current->add_child(pendingKey, pendingList);
            pendingKey.clear();
        }
        pendingList.clear();
        inList = false;
    }

    boost::property_tree::ptree structBegin() {
        stack.push_back(current);
        current = &tempStruct;
        tempStruct.clear();
        return tempStruct;
    }

    void structEnd(const boost::property_tree::ptree&) {
        if (!stack.empty()) {
            current = stack.back();
            stack.pop_back();
        }
    }

    boost::property_tree::ptree root;

private:
    boost::property_tree::ptree* current;
    std::vector<boost::property_tree::ptree*> stack{};
    boost::property_tree::ptree tempStruct{};
    boost::property_tree::ptree pendingList{};
    std::string pendingKey{};
    bool inList{false};
};

inline void writeJson(const InochiSerializer& s, const std::string& path) {
    boost::property_tree::write_json(path, s.root);
}

} // namespace nicxlive::core::serde
