<?php 
  // to look at the _GET params and set a variable if they are set -- used for setting the selected items in the select boxes.
  $type_filter = (isset($_GET['type_filter']) ? trim($_GET['type_filter']) : '');

  $file_type_choices = array('' => 'Select any type',
                             'manuscript' => 'Manuscript', 
                             'image' => 'prints and photographs', 
                             'text - cataloged' => 'Texts that have associated MARC catalog records', 
                             'text - uncataloged' => 'Texts that do NOT have associated MARC catalog records.', 
                             'map' => 'Map', 
                             'pdf' => 'native pdf',
                             'georeferenced map' => 'Georeferenced map object.', 
                             'newspaper - cataloged' => 'Newspapers');
/**
 * $mimetype_choices = array('IMAGE_JPEG' => 'image/jpeg', 'IMAGE_JP2' => 'image/jp2', 'IMAGE_PNG' => 'image/png', 'IMAGE_GIF' => 'image/gif', 'TEXT_CSV' => 'text/csv',
 *                           'TEXT_HTML' => 'text/html', 'TEXT_XML' => 'text/xml', 'APPLICATION_ZIP' => 'application/zip', 'APPLICATION_PDF' => 'application/pdf');
 */

?>
<?php include_once('upitt-workflow-admin-links.tpl.php'); ?>
<?php include_once('upitt-workflow-findobject-form.tpl.php'); ?>
<div style="overflow-x:auto">
  <?php print $table; ?>
</div>
