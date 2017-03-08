<?php 
  // to look at the _GET params and set a variable if they are set -- used for setting the selected items in the select boxes.
  $type_filter = (isset($_GET['type_filter']) ? trim($_GET['type_filter']) : '');
  $collection_filter = (isset($_GET['collection_filter']) ? trim($_GET['collection_filter']) : '');


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
<?php 
  $links[] = l('Active Batches', '/workflow/batches/active');
  $links[] = l('All Batches', '/workflow/batches/all');
  $links[] = l('Collections (legacy)', '/workflow/collection');
  include_once('upitt-workflow-admin-links.tpl.php');
  $breadcrumb = (isset($breadcrumb) ? $breadcrumb : '');
?>
<?php include_once('upitt-workflow-breadcrumb.tpl.php'); ?>
<?php include_once('upitt-workflow-findobject-form.tpl.php'); ?>
<?php print $details; ?>
<h5><?php print $count_rows; ?></h5>
<form action="<?php print $post_to; ?>" method="GET" id="upitt-workflow-objectreport-form">
  <fieldset>
    <legend><span class="fieldset-legend">Filters</span></legend>
    <div class="left_floater">
      <span>Title</span>
      <div class="fieldset-wrapper">
        <input type="text" value="<?php print (isset($_GET['batch_external_id_filter']) ? trim($_GET['batch_external_id_filter']) : '') ?>" name="batch_external_id_filter"> <br>
      </div>
    </div>
<!--
    <div class="left_floater">
      <span>Username</span>
      <div class="fieldset-wrapper">
        <input type="text" value="<?php print (isset($_GET['username_filter']) ? trim($_GET['username_filter']) : '') ?>" name="username_filter"> <br>
      </div>
    </div>
-->
    <div class="left_floater">
      <span>Type</span>
      <div class="fieldset-wrapper">
        <select name="type_filter">
        <?php foreach ($file_type_choices as $key=>$val) { ?>
          <option value="<?php print $key ?>"<?php print (($type_filter == $key) ? ' selected' : '') ?>><?php print $val ?></option>
        <?php } ?>
        </select>
      </div>
    </div>
    <div class="left_floater">
      <span>Collection</span>
      <div class="fieldset-wrapper">
        <select name="collection_filter">
        <?php foreach ($collection_filter_choices as $key=>$val) { ?>
          <option value="<?php print $key ?>"<?php print (($collection_filter == $key) ? ' selected' : '') ?>><?php print $val ?></option>
        <?php } ?>
        </select>
      </div>
    </div>
    <br class="clearfix"/>
    <input type="submit">
  </fieldset>
</form>

<div style="overflow-x:auto">
  <?php print $table; ?>
</div>
