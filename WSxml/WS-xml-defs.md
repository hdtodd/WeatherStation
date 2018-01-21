#WeatherStation XML Data Definitions

The file "weather_data.dtd" contains the definitions of the data elements that the parser is prepared to process and extract/report.  Examining the .dtd file, even without experience in xml DTD definitions, will help understand how to make changes or additions.  Here's a simple guide to the format of the .xml file.

* A conforming .xml file will begin and end with \<samples> and \</samples>
* Between those, the file may contain 0 or more elements marked with \<sample> at the beginning of the entry and \</sample> at the end.  These elements contain the data associated with one sampling of the sensors from which the probe is collecting data.
* A *sample* may contain zero or one \<source_loc> element with a text value.
* A *sample* **must** contain just one \<date_time> element with a text value.
* A *sample* may contain zero or one \<MPL3115A2> element with several required components:
	* \<mpl\_alt> or \<mpl\_alt alt\_unit=ft|m>, with a text value (note that "ft|m" means to put **either** ft or m as the unit of measure; same approach in other unit settings; in this case the measure is feet or meters, ft or m)
	* \<mpl\_press> or \<mpl\_press p\_unit=Pa|mb|inHg>, as text, units being Pascals, millibars, or inches of mercury
	* \<mpl\_temp> or \<mpl\_temp t\_scale=C|F|K>, with temperature value in text and units being Centigrade, Farenheit, or Kelvin
* A *sample* may contain zero or one \<DHT22> element, each with two required components:
	* \<dht\_temp> or \<dht\_temp t\_tscale=C|F|K> with temperature value as text
	* \<dht\_rh> as text representing the relative humidity in percent (%)
* A *sample* may contain zero or more \<DS18> elements, each with two required components:
	* \<ds18\_lbl> as text representing the two-character DS18 internal label
	* \<ds18\_temp> or \<ds18\_temp t_scale=C|F|K> with temperature value as text



