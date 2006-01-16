<!--  $Id$  -->
<?php
    /* 
     *  Set these vars in pages to alter how header works
     *  $includebanner=1      Include the default como banner
     *  $stylesheet="value"   set the style sheet to be "value"
     */ 

    if (!(isset($stylesheet)))
	$stylesheet = "css/live.css";
    if (!(isset($includebanner)))
	$includebanner=0;
    if (!(isset($pagetitle)))
	$pagetitle = "CoMolive! - Intel Research Cambridge";
    else
	$pagetitle = "$pagetitle - CoMolive! - Intel Research Cambridge";
?>

<html>
<head>
  <title><?=$pagetitle?></title>
  <link rel="stylesheet" type="text/css" name="como" href="<?=$stylesheet?>">
  <link rel="shortcut icon" href="images/favicon.ico">
  <meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
  <meta name="description" content="CoMolive!">
</head>

<?php
    /* comolive.conf is the global configuration file  */
    if (file_exists("comolive.conf"))
	include("comolive.conf");
    else {
        print "Please create a comolive.conf file";
        exit;
    }
        

    if ($includebanner) {
	print "<div id=header><a href=$WEBROOT>";
	print "<img src=images/bannerlive.jpg></a></div>\n";
    } 
?>