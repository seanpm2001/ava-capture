# -*- coding: utf-8 -*-
# Generated by Django 1.10.1 on 2017-06-18 01:42
from __future__ import unicode_literals

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('capture', '0023_capturenode_code_version'),
    ]

    operations = [
        migrations.AddField(
            model_name='capturelocation',
            name='bitdepth_avi',
            field=models.IntegerField(default=8),
        ),
        migrations.AddField(
            model_name='capturelocation',
            name='bitdepth_single',
            field=models.IntegerField(default=12),
        ),
    ]
