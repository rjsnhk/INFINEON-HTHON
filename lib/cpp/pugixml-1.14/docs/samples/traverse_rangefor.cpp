#include "pugixml.hpp"

#include <iostream>

int main()
{
    xml_document doc;
    if (!doc.load_file("xgconsole.xml")) return -1;

    xml_node tools = doc.child("Profile").child("Tools");

    // tag::code[]
    for (xml_node tool: tools.children("Tool"))
    {
        std::cout << "Tool:";

        for (xml_attribute attr: tool.attributes())
        {
            std::cout << " " << attr.name() << "=" << attr.value();
        }

        for (xml_node child: tool.children())
        {
            std::cout << ", child " << child.name();
        }

        std::cout << std::endl;
    }
    // end::code[]
}

// vim:et
