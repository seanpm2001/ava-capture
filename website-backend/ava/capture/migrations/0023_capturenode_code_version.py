# -*- coding: utf-8 -*-
# Generated by Django 1.10.1 on 2017-05-30 14:32
from __future__ import unicode_literals

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('capture', '0022_camera_rotation'),
    ]

    operations = [
        migrations.AddField(
            model_name='capturenode',
            name='code_version',
            field=models.IntegerField(default=0),
        ),
    ]
