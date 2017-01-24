<?php
  $links[] = l('Active batches', '/workflow/batches/active');
  $links[] = l('All batches', '/workflow/batches/all');
  include_once('upitt-workflow-admin-links.tpl.php');
?>
<?php include_once('upitt-workflow-findobject-form.tpl.php'); ?>
<h2><?php print $title; ?></h2>

<div>
  <div style="float:left;width:29%">
    <h3>Details</h3>
    <?php print_r($details); ?>
  </div>
  <div style="float:left;width:70%;overflow-x:auto">
    <h3>Status</h3>
    <?php print $status_table; ?>
  </div>
</div>
<br style="clear:both">

<?php print $table; ?>

<form action="<?php print $post_to; ?>" method="GET" id="upitt-workflow-objectreport-form">
  <fieldset class="form-wrapper">
    <legend><span class="fieldset-legend">Filter/s</span></legend>
    <span>Use/s</span>
    <div class="fieldset-wrapper">
      <input type="text" value="<?php print (isset($_GET['use_filter']) ? trim($_GET['use_filter']) : '') ?>" name="use_filter"> <br>
    </div>
    <span>Mime type/s</span>
    <div class="fieldset-wrapper">
      <input type="text" value="<?php print (isset($_GET['mimetype_filter']) ? trim($_GET['mimetype_filter']) : '') ?>" name="mimetype_filter"> <br>
    </div>
    <input type="submit">
  </fieldset>
</form>

<div style="overflow-x:auto">
  <?php print $files_table; ?>
</div>

