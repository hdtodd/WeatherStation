/**
 * ws_xml_inp
 * Master program to read data from an Arduino-based weather probe in XML format
 *   and process into alternative format -- in this demo case, into CSV format.
 *
 * David Todd, August, 2015
 *
 * ws_xml_inp.c : requires libxml2 library
 * To compile this file using gcc you can type
 * gcc `xml2-config --cflags --libs` -o ws_xml_inp ws_xml_inp.c xmlParse.c `xml2-config --libs`
 *
 * xml parsing code from http://xmlsoft.org/examples/tree1.c :
 * synopsis: Navigates a tree to print element names
 * purpose: Parse a file to a tree, use xmlDocGetRootElement() to
 *          get the root element, then walk the document and print
 *          all the samples in document order.
 * usage: ws_xml_inp filename
 * xml parsing code author: Dodji Seketeli
 * copy: see Copyright for the status of this software.
 */
#include <stdio.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

xmlNode* find_element(xmlNode * a_node, xmlChar * target);
xmlNode* find_content(xmlNode * anode);
xmlChar* get_field_value(xmlNode * dia_tree, xmlChar * field_name);
xmlChar *get_attribute_value(xmlNode * a_node, xmlChar * attrib_name);


int main(int argc, char **argv) {
    
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL, *samples_tree, *sample, *mpl, *dht, *ds18;
    xmlChar *date_time, *mpl_press, *mpl_temp, *dht_temp, *dht_rh;
    xmlChar *db_temp, *db_loc;
    
    if (argc != 2) {
      printf("Weather Station XML-input processing program\n");
      printf("Usage: ws_xml_inp <filename>\n");
      return(1);
    };
    
    /*
     * This macro initializes the library and checks potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used.
     */
    LIBXML_TEST_VERSION
    
    /* Parse the file, validate the xml against the DTD, and get the DOM */
    doc = xmlReadFile(argv[1], NULL, XML_PARSE_DTDVALID | XML_PARSE_NOBLANKS);
    
    /* quit if the file couldn't be parsed */
    if (doc == NULL) {
        printf("error: could not parse file %s\n", argv[1]);
        printf("\tUse 'xmllint --valid %s' to validate input file\n", argv[1]);
    }
    
    /* Get the root element node */
    root_element = xmlDocGetRootElement(doc);
    
    /* First, find "samples" tree ELEMENT root node */
    samples_tree = find_element(root_element->parent, (xmlChar *) "samples");
    if (!samples_tree) {
        printf("Weather Station xml input error: libxml2 didn't find 'samples' defined in xml file %s\n", argv[1]);
        printf("Run 'xmllint --valid %s' to check input file for definition of 'samples'\n", argv[1]);
        return(1);
    }
    
    /* Now process the variable number of sample records in the file by walking down the tree */
    for ( (sample=find_element(samples_tree, (xmlChar *) "sample") ); sample; sample=sample->next) {
      /* For each sample node, write the date_time stamp + the data recorded for each sensor */
      printf("(%s", get_field_value(sample, (xmlChar *) "date_time"));
      if (mpl=find_element(sample, (xmlChar *) "MPL3115A2") ) printf(",%s,%s", 
               get_field_value(mpl, (xmlChar *) "mpl_press"),
               get_field_value(mpl, (xmlChar *) "mpl_temp") );
      if (dht=find_element(sample, (xmlChar *) "DHT22") ) printf(",%s,%s", 
               get_field_value(dht, (xmlChar *) "dht_temp"),
	       get_field_value(dht, (xmlChar *) "dht_rh") );
      for ( (ds18=find_element(sample, (xmlChar *) "DS18") ); ds18; ds18=ds18->next) {
	printf(",%s,%s", 
	       get_field_value(ds18, (xmlChar *) "ds18_lbl"),
	       get_field_value(ds18, (xmlChar *) "ds18_temp") );
        };
      printf(")\n");
    }
    
    /*free the document */
    xmlFreeDoc(doc);
    
    /*
     * Free the global variables that may
     * have been allocated by the parser.
     */
    xmlCleanupParser();
    
    return 0;
}

