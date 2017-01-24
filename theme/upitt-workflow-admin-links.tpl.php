<?php include_once('upitt-workflow-admin-links.tpl.php'); ?>
<?php if (count($links) > 0) { ?>
<ul>
  <?php foreach ($links as $link) { ?>
    <li><?php print $link; ?></li>
  <?php } ?>
</ul>
<?php } ?>

