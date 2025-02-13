#include "pugixml.hpp"

#include <iostream>

int main()
{
    xml_document doc;
    if (!doc.load_string("<node><description>Simple node</description><param name='id' value='123'/></node>")) return -1;

    // tag::code[]
    // remove description node with the whole subtree
    xml_node node = doc.child("node");
    node.remove_child("description");

    // remove id attribute
    xml_node param = node.child("param");
    param.remove_attribute("value");

    // we can also remove nodes/attributes by handles
    xml_attribute id = param.attribute("name");
    param.remove_attribute(id);
    // end::code[]

    doc.print(std::cout);
}

// vim:et
