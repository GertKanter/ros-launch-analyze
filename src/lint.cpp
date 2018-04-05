#include "ros_launch_lint/load_roslaunch.h"
#include "ros_launch_lint/node_tree.h"
#include "ros_launch_lint/report.h"

void print_usage() {
    ROS_INFO("usage: ros-launch-lint [OPTIONS] LAUNCH-FILE");
}

struct Options {
    Options() {}
    Options(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string s = argv[i];
            if (s == "--print-dot")          print_dot = true;
            if (s == "--print-node-tree")    print_node_tree = true;
            if (s == "--print-topics")       print_topics = true;
            if (s == "--no-print-dot")       print_dot = false;
            if (s == "--no-print-node-tree") print_node_tree = false;
            if (s == "--no-print-topics")    print_topics = false;

            if (s == "--analyze-sandbox")    analyze_sandbox = true;
            if (s == "--no-analyze-sandbox") analyze_sandbox = false;
        }
    }

    bool print_dot = false;
    bool print_node_tree = true;
    bool print_topics = true;

    bool analyze_sandbox = true;
};

int main(int argc, char* argv[]) {

    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    Options op {argc-1, argv};
    auto filename = argv[argc-1];

    // load the launch file and resolve all includes and substitutions
    XMLDocument idoc, doc;
    doc.LoadFile(filename);
    include_all(idoc, resolve_roslaunch_substitutions(doc));

    // create node tree
    NodeListVisitor nv(filename);
    doc.Accept(&nv);

    // decorate node tree with data from the sandboxed execution
    if (op.analyze_sandbox)
        nv.query_topics();

    // create report
    if (op.print_dot)
        print_dot(nv.node_tree());

    if (op.print_node_tree)
        print_node_tree(nv.node_tree());

    if (op.print_topics)
        print_topics(nv.node_tree());

    return 0;
}
