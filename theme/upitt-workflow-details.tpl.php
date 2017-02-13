<?php foreach ($details as $key => $detail) { ?>
  <strong><?php print ucwords($key); ?></strong><br><?php print $detail; ?><br style="padding-bottom:4px">
<?php } ?>

<?php print ($edit_item_link) ? "<hr>" . $edit_item_link : ''; ?>

