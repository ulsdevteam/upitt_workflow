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
  <div style="float:left;width:66%;overflow-x:auto;padding-left:2%">
    <h3>Status Transactions</h3>
    <?php print $status_table; ?>
  </div>
</div>
<br style="clear:both">

<?php if ($table) { echo "<h3>TABLE</h3>"; } ?>
<?php print $table; ?>

<form action="<?php print $post_to; ?>" method="GET" id="upitt-workflow-objectreport-form">
  <fieldset class="form-wrapper">
    <legend><span class="fieldset-legend">Filter/s</span></legend>
    <div class="left_floater">
      <span>Use/s</span>
      <div class="fieldset-wrapper">
        <input type="text" value="<?php print (isset($_GET['use_filter']) ? trim($_GET['use_filter']) : '') ?>" name="use_filter"> <br>
      </div>
    </div>
    <div class="left_floater">
      <span>Mime type/s</span>
      <div class="fieldset-wrapper">
        <input type="text" value="<?php print (isset($_GET['mimetype_filter']) ? trim($_GET['mimetype_filter']) : '') ?>" name="mimetype_filter"> <br>
      </div>
    </div>
    <div class="left_floater">
      <br class="clearfix">
      <input type="submit">
    </div>
  </fieldset>
</form>

<div style="overflow-x:auto">
  <?php print $files_table; ?>
</div>

