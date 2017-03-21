<?php include_once('upitt-workflow-breadcrumb.tpl.php'); ?>
<?php include_once('upitt-workflow-findobject-form.tpl.php'); ?>

<h3>Tasks</h3>
<ul>
  <?php foreach ($task_links as $task_link): ?>
    <li><?php echo $task_link; ?></li>
  <?php endforeach; ?>
</ul> 

<h3>Edit Lists</h3>
<?php include_once('upitt-workflow-admin-links.tpl.php'); ?>

