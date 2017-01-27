<?php include_once('upitt-workflow-breadcrumb.tpl.php'); ?>
<?php include_once('upitt-workflow-findobject-form.tpl.php'); ?>

<div style="overflow-x:auto">
  <?php print $markup; ?>
</div>

<?php include_once('upitt-workflow-admin-links.tpl.php'); ?>

<h3>Collections Needing Rebuilding</h3>
<ul>        
  <li>pitttextdir</li>
  <li>pittpress</li>
  <li>pittmiscpubs</li>
  <li>pittchancellor</li>
  <li>pitttext</li>
  <li>ulstext</li>          
</ul>
<i style="font-size:smaller">these are hard-coded in the template for now </i>


<h3>Tasks</h3>
<ul>
  <li class="big"><a href="/workflow/batch/new/">Create New Batch</a></li>
  <li class="big"><a href="/workflow/collection_building/">[online] Collection Building</a></li>
  <li class="big"><a href="/workflow/status/change/">Quick Status Change</a></li>
</ul> 
<i style="font-size:smaller">these are hard-coded in the template for now </i>

