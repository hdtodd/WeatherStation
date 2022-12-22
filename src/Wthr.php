<!-- Template for a web site home page that also displays the history of
       WeatherStation-collected data as a graph
     Uses Google Charts for the display
     Written by HDTodd, January, 2016; updated for DS18 temps August, 2017
       borrowing heavily from numerous prior authors 
     Updated 2018.09.17 for Apache 2.4.25 and to use 
       Google AJAX JQuery API 3.3.1 (https://developers.google.com/speed/libraries/)
-->
<?php
$HISTORY="'-168 hours'";	   //period of time over which to display temps
$DB_LOC="/var/databases/"; //location of the sqlite3 db
$DB_NAME="WeatherData.db";   //name of sqlite3 db
?>

<?php
// First, PHP code to populate an array with the [time,temp] data pairs
//   and create a JSON array for the Javascript below

$db = new PDO('sqlite:' . $DB_LOC . $DB_NAME) 
      	  or die('Cannot open database ' . $DB_NAME);
$query = "SELECT * FROM ProbeData  WHERE date_time>datetime('now',$HISTORY)"; 
foreach ($db->query($query) as $row) 
  $chart_array[]=array((string)$row['date_time'],(real)$row['ds18_2_temp'],(int)$row['mpl_press']); 
$query = "SELECT * FROM ProbeData ORDER BY date_time DESC LIMIT 1";
foreach ($db->query($query) as $row) {
  $last_time=(string)$row['date_time'];
  $last_lbl1=json_encode( (string)$row['ds18_1_lbl']);
  $last_temp1=json_encode( (real)$row['ds18_1_temp']);
  $last_lbl2=json_encode( (string)$row['ds18_2_lbl']);
  $last_temp2=json_encode( (real)$row['ds18_2_temp']);
  $last_press=json_encode( (int)$row['mpl_press'] );
}
//Now convert to a JSON array for the Javascript
$temp_data=json_encode($chart_array);
//For debugging, uncomment the following
//echo $temp_data;
?>

<!-- Here's the HTML code for the site, followed by the JS component for the chart -->
<html>
<center>
<h1>The <?php echo gethostname() ?> Meterological Data Web Site</h1>
<h2>Current conditions at <?php echo $last_time ?></br> 
Temps: <?php echo $last_lbl1 ?>=<?php echo $last_temp1 ?>°F and 
<font color="blue"><?php echo $last_lbl2 ?>=<?php echo $last_temp2 ?>°F</font> 
at <font color="red">Pressure = <?php echo $last_press ?> Pa
= <?php echo round($last_press/100000.0*29.53,2,PHP_ROUND_HALF_UP) ?> in</font></h2>
<p>
  <head>
    <!--Load the AJAX API-->
    <script type="text/javascript" src="https://www.google.com/jsapi"></script>
    <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js"></script>

    <script type="text/javascript">
      google.load("visualization", "1", {packages:["corechart"]});
      google.setOnLoadCallback(drawChart);
      function drawChart() {
        var data = new google.visualization.DataTable();
        data.addColumn("string","DateTime");
	data.addColumn("number","Outside Temp (°F)");
	data.addColumn("number","Pressure (Pa)");
        data.addRows( <?php echo $temp_data ?>);
        var options = {
          title: 'Outside Temp and Barometric Pressure History',
	  series: {
	    0: {targetAxisIndex: 0},
	    1: {targetAxisIndex: 1}
	  },
	  vAxes: {
	    0: {title: 'OU Temp (°F)'},
	    1: {title: 'Pressure (Pa)'}
	  }  
        };
        var chart = new google.visualization.LineChart(document.getElementById('chart_div'));
        chart.draw(data, options);
      }
    </script>
  </head>
  <body>
    <!--Div that will hold the line graph-->
    <div id="chart_div" style="width: 900px; height: 500px;"></div>
  </body>
</center>
</html>
