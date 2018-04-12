#include "ros_launch_lint/xml_annotation.h"

std::vector<Port> parse_annotation(const XMLComment& elt, NodeTree::tree_t::tree_node* tnode) {
    std::vector<Port> node_ports;

    // try to parse the comment as xml
    std::string comment = elt.Value();
    XMLDocument comment_doc;
    if (comment_doc.Parse(comment.c_str(), comment.size()) == XML_NO_ERROR) {
        const XMLNode* topic_node = comment_doc.FirstChild();
        if (!topic_node || !topic_node->ToElement() || (std::string(topic_node->ToElement()->Name()) != "topics" && std::string(topic_node->ToElement()->Name()) != "services")) {
            ROS_INFO("Invalid xml annotation structure");
            return std::vector<Port>();
        }

        for (const XMLNode* node = topic_node->FirstChild(); node != nullptr; node = node->NextSibling()) {
            const XMLElement* elt = node->ToElement();

            if (elt) {
                std::string cls = elt->Attribute("class");
                Port p {get_absolute_path(tnode, elt->Attribute("name")),
                        elt->Attribute("type"),
                        -1,
                        (cls == "pub" ? Port::PUBLISHER
                      : (cls == "sub" ? Port::SUBSCRIBER
                      : (cls == "adv" ? Port::SERVICE_ADVERTISE
                      : (cls == "call" ? Port::SERVICE_CLIENT
                      : Port::NONE))))};
                node_ports.push_back(p);
            }
        }
    } else {
        ROS_INFO("Comment found, but failed to parse xml");
    }

    return node_ports;
}

void xml_annotation(NodeTree& tree) {
    for (auto it = tree.nodes.begin(); it != tree.nodes.end(); ++it) {

        if (!it->type.empty()) {
            it->ports.push_back(Port {"/rosout", "rosgraph_msgs/Log", -1, Port::PUBLISHER});
            it->ports.push_back(Port {get_absolute_path(it.node, it->name + "/get_loggers"), "roscpp/GetLoggers", -1, Port::SERVICE_ADVERTISE});
            it->ports.push_back(Port {get_absolute_path(it.node, it->name + "/set_logger_level"), "roscpp/SetLoggerLevel", -1, Port::SERVICE_ADVERTISE});
        }

        XMLConstHandle xml = it->xml;
        for (auto node = xml.FirstChild().ToNode(); node != nullptr; node = node->NextSibling()) {
            if (node->ToComment())
                for (auto port : parse_annotation(*node->ToComment(), it.node))
                    it->ports.push_back(port);
        }
    }
}

template <typename T>
bool is_equal(const T& a, const T& b,
              std::function<bool(const typename T::value_type&, const typename T::value_type)> elt_eq
                    = std::equal_to<typename T::value_type>()) {
    //TODO: avoid doubled comparisons

    for (const auto& a_elt : a) {
        bool found = false;
        for (const auto& b_elt : b) {
            if (elt_eq(a_elt, b_elt)) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }

    for (const auto& b_elt : b) {
        bool found = false;
        for (const auto& a_elt : a) {
            if (elt_eq(b_elt, a_elt)) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }

    return true;
}

bool diff_ports(const NodeTree& a, const NodeTree& b, std::ostream& output) {
    bool mismatch = false;

    bool nodes_found = is_equal(a.nodes, b.nodes, [&](const NodeDesc& na, const NodeDesc& nb) {
        if (na.name != nb.name || na.path != nb.path)
            return false;

        bool ports_equal = is_equal(na.ports, nb.ports);

        if (!ports_equal) {
            output << "Topic mismatch for node " << na.name << std::endl;
            output << na << std::endl << nb << std::endl;
        }

        return true;
    });

    if (!nodes_found) {
        output << "Mismatch due to missing node" << std::endl;
    }

    return nodes_found && !mismatch;
}
