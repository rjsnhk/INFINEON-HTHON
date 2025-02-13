#include "pugixml.hpp"

#include <iostream>

int main()
{
    xml_document doc;
    if (!doc.load_file("xgconsole.xml")) return -1;

// tag::code[]
    xpath_node_set tools = doc.select_nodes("/Profile/Tools/Tool[@AllowRemote='true' and @DeriveCaptionFrom='lastparam']");

    std::cout << "Tools:\n";

    for (xpath_node_set::const_iterator it = tools.begin(); it != tools.end(); ++it)
    {
        xpath_node node = *it;
        std::cout << node.node().attribute("Filename").value() << "\n";
    }

    xpath_node build_tool = doc.select_node("//Tool[contains(Description, 'build system')]");

    if (build_tool)
        std::cout << "Build tool: " << build_tool.node().attribute("Filename").value() << "\n";
// end::code[]
}

// vim:et
