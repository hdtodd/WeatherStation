#!/usr/bin/env python
# ========================================================
# Name: Wthr.py
# --------------------------------------------------------
# Author:
#  HDTodd based on work of prior authors,
#  Most recently: Craig Deaton
# --------------------------------------------------------
# Purpose:
#    To query and retrieve data from WeatherData.db and graph
#    for an internval period selected by the user
# --------------------------------------------------------
# Environment:
#    Runs as a Python CGI script and connects to a SQLite3 database
# --------------------------------------------------------
# Invocation:
#    http://localhost/cgi-bin/Wthr.py
# --------------------------------------------------------
# Parameters:
#    Form fields can be used to select past number of hours to graph
# --------------------------------------------------------
# Output:
#    Generated HTML for title, form data, javascript for chart from Google Charts, and
#    summary stats for min, max, avg temps for the selected period.  All displayed via browser
# --------------------------------------------------------
# Modifications:
# Version	Date		Name	Description
# ------------------------------------------------------------------------
# 0001		1-Oct-2015	CD	Original script from https://github.com/Pyplate/rpi_temp_logger
# 001	       11-Oct-2015	CD	modified script for this particular environment, pointed
#                                       dbname to proper file name,
# 002          23-Jan-2016      HDT     Modified for WeatherData 
# ========================================================

import sqlite3
import sys
import cgi
import cgitb
import socket

# global variables
speriod=(15*60)-1

# -----
# 001
# -----


dbname='/var/databases/WeatherData.db'

# convert rows from database into a javascript table
def create_table(rows):
    chart_table=""

    for row in rows[:-1]:
        rowstr="['{0}', {1}],\n".format(str(row[0]),str(row[1]))
        chart_table+=rowstr

    row=rows[-1]
    rowstr="['{0}', {1}]\n".format(str(row[0]),str(row[1]))
    chart_table+=rowstr

    return chart_table

# main function
# This is where the program starts 

def main():

    cgitb.enable()

    # get data from the database for last 24 hours
    # if you choose not to use the SQLite3 database, construct you record array in a similar manner to the output
    # from the cursor fetch all rows
    #      date-time string  temperature string \n 
    #          and then skip the checking for len(records) and doing the chart_table function,  just call the
    #  

    conn=sqlite3.connect(dbname)
    curs=conn.cursor()
    option = 336
    curs.execute("SELECT * FROM ProbeData WHERE date_time>datetime('now','-%s hours')" % option)
    records=curs.fetchall()
    conn.close()

    # -------------------------------------
    # print the HTTP header
    # -------------------------------------
    
    print "Content-type: text/html\n\n"

    # -------------------------------------
    # convert record rows to table if anyreturned
    # -------------------------------------
    if len(records) != 0:
        # convert the data into a table
        table=create_table(records)
    else:
        print "No data found"
        sys.stdout.flush()
        return

    # -------------------------------------
    # start printing the page
    # -------------------------------------
    print "<html>"
 
    # -------------------------------------
    # print the HTML head section
    # -------------------------------------

    print "<head>"
    print "    <title>"
    print "WeatherStation Data for host " + socket.gethostname()
    print "    </title>"

    # -------------------------------------
    # format and print the graph
    # -------------------------------------
    # print_graph_script(table)

    # google chart snippet
    chart_code="""
    <script type="text/javascript" src="https://www.google.com/jsapi"></script>
    <script type="text/javascript">
      google.load("visualization", "1", {packages:["corechart"]});
      google.setOnLoadCallback(drawChart);
      function drawChart() {
        var data = google.visualization.arrayToDataTable([
          ['Time', 'Pressure (Pa)'],
%s
        ]);

        var options = {
          title: 'Pressure (Pa)'
        };

        var chart = new google.visualization.LineChart(document.getElementById('chart_div'));
        chart.draw(data, options);
      }
    </script>"""

    print chart_code % (table)
    print "</head>"

    # ---------------------------------------
    # print the page body
    # ---------------------------------------

    print "<body>"
    print "<h1>'" + socket.gethostname() + "' WeatherStation Data Log</h1>"
    print "<hr>"



    # ------------------------------------
    # show_graph()
    # ------------------------------------

    # print the div that contains the graph
    print "<h2>Barometric Pressure Chart</h2>"
    print '<div id="chart_div" style="width: 900px; height: 500px;"></div>'

   
    print "<hr>"
 
    print "</body>"
    print "</html>"

    conn.close()

    sys.stdout.flush()

if __name__=="__main__":
    main()


