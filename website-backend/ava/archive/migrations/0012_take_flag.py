# -*- coding: utf-8 -*-
# Generated by Django 1.10.1 on 2017-04-27 20:13
from __future__ import unicode_literals

from django.db import migrations
import model_utils.fields


class Migration(migrations.Migration):

    dependencies = [
        ('archive', '0011_auto_20170405_1049'),
    ]

    operations = [
        migrations.AddField(
            model_name='take',
            name='flag',
            field=model_utils.fields.StatusField(choices=[(0, 'dummy')], default='none', max_length=100, no_check_for_status=True),
        ),
    ]
