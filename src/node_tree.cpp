#include "ros_launch_lint/node_tree.h"

NodeListVisitor::NodeListVisitor(const std::string& root_xml) {
    nss.push(Namespace {"/"});
    file_stack.push(root_xml);
    tree.nodes.insert(tree.nodes.begin(), NodeDesc {"/", "", "", "", root_xml});
    it = create_path(nss.top().name);
}

bool NodeListVisitor::VisitExit (const XMLElement &elt) {
    if (std::string(elt.Name()) == "group" ||
        std::string(elt.Name()) == "test" ||
        std::string(elt.Name()) == "include") {

        nss.pop();
        it = create_path(nss.top().name);
    }

    if (std::string(elt.Name()) == "include") {
        file_stack.pop();
    }

    if (std::string(elt.Name()) == "node") {

        // save Namespace and all remaps for this node
        auto current_nss = nss;
        Namespace result_ns = current_nss.top();
        current_nss.pop();

        for (; !current_nss.empty(); current_nss.pop()) {
            for (const auto& remap : current_nss.top().remaps)
                result_ns.remaps.push_back(remap);
        }

        // remove node from namespace stack
        nss.pop();
        it = create_path(nss.top().name);

        // create child node in NodeTree
        tree.nodes.append_child(it, NodeDesc {elt.Attribute("name"),
                                              get_absolute_path(it.node, it->name),
                                              elt.Attribute("type"),
                                              elt.Attribute("pkg"),
                                              file_stack.top(),
                                              result_ns,
                                              private_params,
                                              elt.Attribute("args") ? elt.Attribute("args") : "",
                                              std::vector<Port>(),
                                              elt});
        // reset private parameters
        private_params.clear();
    } else if (std::string(elt.Name()) == "param"
            && elt.Parent() && std::string(elt.Parent()->ToElement()->Name()) != "node") {

        // we are not inside a node -> the param is global
        for (auto& p : private_params) {
            p.first = get_absolute_path(it.node, it->name + p.first);
            tree.global_params.push_back(p);
        }
        private_params.clear();
    }

    return true;
}

bool NodeListVisitor::VisitEnter (const XMLElement &elt, const XMLAttribute *) {
    if (std::string(elt.Name()) == "param") {
        if (elt.Attribute("name") && elt.Attribute("value")) {
            private_params.push_back(std::make_pair(elt.Attribute("name"),
                                                    elt.Attribute("value")));
        }
    }

    if (std::string(elt.Name()) == "remap") {
        nss.top().remaps.emplace_back(std::make_pair(elt.Attribute("from"),
                                                     elt.Attribute("to")));
    }

    if (std::string(elt.Name()) == "group" ||
        std::string(elt.Name()) == "node" ||
        std::string(elt.Name()) == "test" ||
        std::string(elt.Name()) == "include") {

        std::string ns = nss.top().name;

        if (elt.Attribute("ns")) {
            ns = elt.Attribute("ns");
        }

        if (ns[0] != '/') {
            ns = nss.top().name + ns;
        }

        if (ns[ns.size()-1] != '/') {
            ns = ns + '/';
        }

        nss.push(Namespace {ns});

        if (elt.Attribute("ns")) {
            it = create_path(ns);
        }
    }

    if (std::string(elt.Name()) == "include") {
        file_stack.push(elt.Attribute("file"));
    }
    return true;
}

NodeTree::tree_t::iterator NodeListVisitor::create_path(const std::string& path) {
    NodeTree::tree_t::pre_order_iterator tree_it = tree.nodes.begin();

    size_t pos = 0;
    while (pos != std::string::npos) {
        auto new_pos = path.find('/', pos+1);
        std::string ns = path.substr(pos+1, new_pos-pos);
        pos = new_pos;
        if (ns.empty())
            continue;

        unsigned num_children = tree_it.number_of_children();
        if (num_children == 0) {
            tree_it = tree.nodes.append_child(tree_it,
                                              NodeDesc {ns, get_absolute_path(tree_it.node, tree_it->name), "", "", file_stack.top()});
        } else {
            bool found = false;
            for (auto child_it = tree_it.begin(); child_it != tree_it.end(); ++child_it) {
                if (child_it->name == ns) {
                    tree_it = child_it;
                    found = true;
                    break;
                }
            }
            if (!found) {
                tree_it = tree.nodes.append_child(tree_it,
                                                  NodeDesc {ns, get_absolute_path(tree_it.node, tree_it->name), "", "", file_stack.top()});
            }
        }
    }

    return tree_it;
}

std::string resolve_remaps(const NodeDesc& node, std::string path) {

    ROS_INFO_STREAM("Resolving remaps for " << path);

    for (const auto& p : node.ns.remaps) {
        if (path == p.first || path == (node.ns.name + p.first)) {
            path = p.second;
            ROS_INFO_STREAM("...remapped to " << path);
            return path;
        } else {
            ROS_INFO_STREAM("remap " << p.first << " -> " << p.second << " does not match");
        }
    }

    ROS_INFO_STREAM("...no remap found");

    return path;
}

std::ostream& operator<< (std::ostream& out, const NodeDesc& desc) {
    out << desc.path << desc.name << std::endl;

    for(const auto& p : desc.params)
        out << "  " << p.first << "=" << p.second << std::endl;

    out << std::endl;

    for(const auto& p : desc.ports) {
        out << "  " << p.name << " [" << p.data_type << "] class:";

        switch(p.type) {
        case Port::NONE:
            out << "none";
            break;
        case Port::PUBLISHER:
            out << "pub";
            break;
        case Port::SUBSCRIBER:
            out << "sub";
            break;
        case Port::SERVICE_ADVERTISE:
            out << "adv";
            break;
        case Port::SERVICE_CLIENT:
            out << "call";
            break;
        }

        out << std::endl;
    }

    return out;
}
