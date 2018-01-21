/**
 * Procedure to parse an XML input specification for a set of diatomic
 * molecular orbital calculations to be performed, including optimization or
 * plotting series specifications, and generate the input for the diatom
 * program.
 *
 * David Todd, April, 2014.
 *
 * dia-xml-inp.c : requires libxml2 library
 * To compile this file using gcc you can type
 * gcc `xml2-config --cflags --libs` -o dia-xml-inp dia-xml-inp.c `xml2-config --libs`
 *
 * xml parsing code from http://xmlsoft.org/examples/tree1.c :
 * synopsis: Navigates a tree to print element names
 * purpose: Parse a file to a tree, use xmlDocGetRootElement() to
 *          get the root element, then walk the document and print
 *          all the element name in document order.
 * usage: tree1 filename_or_URL
 * test: tree1 test2.xml > tree1.tmp && diff tree1.tmp $(srcdir)/tree1.res
 * author: Dodji Seketeli
 * copy: see Copyright for the status of this software.
 */
#include <stdio.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

/**
 * Finds and returns the address of the ELEMENT node labeled by the
 * string "target"; returns NULL if not found.
 * Starts at the CHILDREN list pointed to by the parent node and
 * NOT at the parent node itself (NOTE WELL!)
 **/
xmlNode* find_element(xmlNode * a_node, xmlChar * target)
{
    xmlNode *cur_node = NULL;
    
    for (cur_node = a_node->children; cur_node; cur_node = cur_node->next)
        if ( (cur_node->type == XML_ELEMENT_NODE) && xmlStrEqual(cur_node->name,(const xmlChar*) target) )
            return(cur_node);
    return(NULL);
}

/**
 * Finds and returns the address of the TEXT node, containing the content for,
 * the ELEMENT node supplied as parameter 'a_node'; returns NULL if not found.
 **/
xmlNode* find_content(xmlNode * a_node)
{
    xmlNode *con_node = NULL;
    
    for (con_node = a_node->children; con_node; con_node = con_node->next)
        if (con_node->type == XML_TEXT_NODE ) return(con_node);
    return(NULL);
}

/**
 * Retrieves a field from the tree
 * Supply the top node of the 'diatomic' tree and
 * a string with the name of the field you want and get back
 * a pointer to the text value associated with that field name
 **/
xmlChar *get_field_value(xmlNode * dia_tree, xmlChar * field_name) {
    xmlNode *cur_node=NULL, *con_node=NULL;
    
    if ( (cur_node=find_element(dia_tree, field_name)) )
        if ( (con_node=find_content(cur_node)) )
            return(con_node->content);
        else {
            printf("Couldn't find '%s' TEXT node\n", field_name);
            return(NULL);
        }
        else {
            printf("Couldn't find '%s' ELEMENT node\n", field_name);
            return(NULL);
        }
}

/**
 * Retrieves an ATTRIBUTE associated with an ELEMENT node.
 * Supply an ELEMENT node of the 'diatomic' tree and
 * a string with the name of the ATTRIBUTE you want and get back
 * a pointer to the text value associated with that ATTRIBUTE name
 **/
xmlChar *get_attribute_value(xmlNode * a_node, xmlChar * attrib_name) {
    
    xmlAttr *cur_node = NULL;
    xmlNode *con_node = NULL;
    
    for (cur_node = a_node->properties; cur_node; cur_node = cur_node->next)
        if ( (cur_node->type == XML_ATTRIBUTE_NODE) && xmlStrEqual(cur_node->name,(const xmlChar*) attrib_name) )
            if ( (con_node=find_content((xmlNode *) cur_node)) )
                return(con_node->content);
    //    printf("Couldn't find '%s' ATTRIBUTE node\n", attrib_name);
    return(NULL);
}

